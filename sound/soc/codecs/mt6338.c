// SPDX-License-Identifier: GPL-2.0
//
// mt6338.c  --  mt6338 ALSA SoC audio codec driver
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Ting-Fang Hou <Ting-Fang.Hou@mediatek.com>

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/iio/consumer.h>
#include <linux/nvmem-consumer.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/regulator/consumer.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/soc.h>
#include <sound/core.h>

#include "mt6338.h"
#if IS_ENABLED(CONFIG_SND_SOC_MT6338_ACCDET)
#include "mt6338-accdet.h"
#endif

#if IS_ENABLED(CONFIG_MT6685_AUDCLK)
#include <linux/mfd/mt6685-audclk.h>
#endif

#define MTKAIFV4_SUPPORT
#define MAX_DEBUG_WRITE_INPUT 256
#define CODEC_SYS_DEBUG_SIZE (1024 * 48) // 32K
/* #define MT6338_TOP_DEBUG */
/* #define MT6338_OTHER_DEBUG */
/* #define MT6338_GSRC_DEBUG */
/* #define MT6338_IIR_DEBUG */
/* #define MT6338_ULCF_DEBUG */
#define MT6338_NLE_DEBUG
/* #define MT6338_XTALK_DEBUG */
/* #define MT6338_SCF_DEBUG */
/* #define MT6338_VOW_DEBUG */
/* #define MT6338_ACCDET_DEBUG */
#define MT6338_GAIN_DEBUG
/* #define NLE_IMP */

static ssize_t mt6338_codec_sysfs_read(struct file *filep, struct kobject *kobj,
				       struct bin_attribute *attr,
				       char *buf, loff_t offset, size_t size);
static ssize_t mt6338_codec_sysfs_write(struct file *filp, struct kobject *kobj,
					struct bin_attribute *bin_attr,
					char *buf, loff_t off, size_t count);

static void keylock_set(struct mt6338_priv *priv);
static void keylock_reset(struct mt6338_priv *priv);

static int mt6338_key_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(component);
	unsigned int value;

	ucontrol->value.integer.value[0] =
	regmap_read(priv->regmap, MT6338_TOP_DIG_WPK, &value);
	dev_dbg(priv->dev, "%s(), value = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int mt6338_key_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv  = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), value = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		keylock_set(priv);
	else
		keylock_reset(priv);
	return 0;
}

unsigned int mt6338_etdm_rate_transform(unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MT6338_AFE_ETDM_8000HZ;
	case 11025:
		return MT6338_AFE_ETDM_11025HZ;
	case 16000:
		return MT6338_AFE_ETDM_16000HZ;
	case 22050:
		return MT6338_AFE_ETDM_22050HZ;
	case 32000:
		return MT6338_AFE_ETDM_32000HZ;
	case 44100:
		return MT6338_AFE_ETDM_44100HZ;
	case 48000:
		return MT6338_AFE_ETDM_48000HZ;
	case 88200:
		return MT6338_AFE_ETDM_88200HZ;
	case 96000:
		return MT6338_AFE_ETDM_96000HZ;
	case 176400:
		return MT6338_AFE_ETDM_176400HZ;
	case 192000:
		return MT6338_AFE_ETDM_192000HZ;
	case 384000:
		return MT6338_AFE_ETDM_384000HZ;
	default:
		return MT6338_AFE_ETDM_48000HZ;
	}
}

unsigned int mt6338_rate_transform(unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MT6338_ADDA_8000HZ;
	case 11025:
		return MT6338_ADDA_11000HZ;
	case 16000:
		return MT6338_ADDA_16000HZ;
	case 22050:
		return MT6338_ADDA_22000HZ;
	case 32000:
		return MT6338_ADDA_32000HZ;
	case 44100:
		return MT6338_ADDA_44000HZ;
	case 48000:
		return MT6338_ADDA_48000HZ;
	case 96000:
		return MT6338_ADDA_96000HZ;
	case 192000:
		return MT6338_ADDA_192000HZ;
	default:
		return MT6338_ADDA_48000HZ;
	}
}

unsigned int mt6338_dlsrc_rate_transform(unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MT6338_DLSRC_8000HZ;
	case 16000:
		return MT6338_DLSRC_16000HZ;
	case 32000:
		return MT6338_DLSRC_32000HZ;
	case 48000:
		return MT6338_DLSRC_48000HZ;
	case 96000:
		return MT6338_DLSRC_96000HZ;
	case 192000:
		return MT6338_DLSRC_192000HZ;
	default:
		return MT6338_DLSRC_48000HZ;
	}
}

unsigned int mt6338_voice_rate_transform(unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MT6338_VOICE_8000HZ;
	case 16000:
		return MT6338_VOICE_16000HZ;
	case 32000:
		return MT6338_VOICE_32000HZ;
	case 48000:
		return MT6338_VOICE_48000HZ;
	case 96000:
		return MT6338_VOICE_96000HZ;
	case 192000:
		return MT6338_VOICE_192000HZ;
	default:
		return MT6338_VOICE_48000HZ;
	}
}
#if IS_ENABLED(CONFIG_MT6338_EFUSE)
static void mt6338_get_hw_ver(struct mt6338_priv *priv)
{
	int ret = 0;
	int value, fab;
	unsigned short efuse_val = 0;

	ret = nvmem_device_read(priv->hp_efuse, 0x8, 1, &efuse_val);
	value = (efuse_val >> 5) & 0x3;

	ret = nvmem_device_read(priv->hp_efuse, 0xE, 1, &efuse_val);
	fab = (efuse_val >> 5) & 0x3;

	priv->hw_ver = value;
	pr_info("%s() mt6338 fab=%d, hw_ver= %d\n",
		__func__, fab, priv->hw_ver);
}

#ifdef NLE_IMP
int cal_pow(int exp)
{
	int i = 0;
	int abs_exp = 0;
	int result = 0;

	abs_exp = (exp < 0) ? -exp : exp;

	for (i = 0; i < abs_exp; i++) {
		if (exp > 0)
			result = 1.0018 * result;
		else
			result = (0.9982) * result;
	}
	return result;
}

static const int ref_value[MT6338_NLE_GAIN_STAGE] = {
	0, 0, 16, 32, 64, 128, 192, 256
};

static void calculate_gain_stage(struct mt6338_priv *priv)
{
	/* int S_value = 0; */
	int R_value = 0;
	int G_value = 0;
	int g0 = 0;
	int L_value = 0;
	unsigned int R_load = 16;
	int step = 0;
	int i = 0;
	unsigned int temp = 0;

	for (i = 0; i < MT6338_NLE_GAIN_STAGE * 2; i++) {
		step = i % 8;
		L_value = (i < 8) ?
					priv->nle_trim.L_LN[step] : priv->nle_trim.R_LN[step];

		if (step == 0) {
			g0 = cal_pow(L_value);
			G_value = g0;
		} else if (step == 1) {
			R_value = (unsigned int)L_value >> 5;
			G_value = DIV_ROUND_CLOSEST(g0 * (R_load + R_value), R_load);
		} else {
			/* G2~G7 */
			temp = (unsigned int)L_value >> 7;
			R_value = ref_value[step] * (temp + 1);
			G_value = DIV_ROUND_CLOSEST(g0 * (R_load + R_value), R_load);
		}
		if (i < 8)
			priv->nle_trim.L_GS[step] = G_value * 65536;
		else
			priv->nle_trim.R_GS[step] = G_value * 65536;

		pr_info("%s() G0 =%d => 0x%x\n",
		__func__, G_value, (unsigned int)G_value);
	}
}

static void mt6338_get_ln_gain(struct mt6338_priv *priv)
{
	int i = 0, ret = 0;
	int value = 0, sign = 0;
	unsigned int addr = 0;
	unsigned short efuse_val = 0;

	/* HPL_LNx: [508:515]...[564:571]
	 * HPR_LNx: [572:579]...[628:635]
	 * 508 / 8 = 63.x
	 * efuse read L : ( 576 - 504(0x1f8) /8 ) +1 = 10
	 * efuse read R : (640 - 568 /8 ) +1 = 10
	 */
	for (i = 0; i < MT6338_NLE_GAIN_STAGE*2 ; i++) {
		addr = 0x1f8 + (unsigned int)i;
		pr_info("%s() efuse addr [%d] = 0x%x\n",
					__func__, i, addr);
		ret = nvmem_device_read(priv->hp_efuse, addr, 0x2, &efuse_val);
		sign = (efuse_val >> 11) & 0x1;
		value = (efuse_val >> 4) & 0x7f;
		value = sign ? -value : value;
		if (i < MT6338_NLE_GAIN_STAGE) {
			priv->nle_trim.L_LN[i] = value;
			pr_info("%s() nle_trim.L_LN[%d]= %d\n",
					__func__, i, priv->nle_trim.L_LN[i]);
		} else {
			priv->nle_trim.R_LN[i-MT6338_NLE_GAIN_STAGE] = value;
			pr_info("%s() nle_trim.R_LN[%d]= %d\n",
					__func__, i-MT6338_NLE_GAIN_STAGE,
					priv->nle_trim.R_LN[i-MT6338_NLE_GAIN_STAGE]);
		}
	}
	calculate_gain_stage(priv);
}
#endif
#endif

/* static function declaration */
static void mt6338_set_gpio_smt(struct mt6338_priv *priv)
{
	/* set gpio SMT mode */
#if defined(MTKAIFV4_SUPPORT)
	regmap_update_bits(priv->regmap, MT6338_SMT_CON0, 0xf0, 0xf0);
#else
	regmap_update_bits(priv->regmap, MT6338_SMT_CON1, 0x7, 0x7);
#endif
}

static void mt6338_set_gpio_driving(struct mt6338_priv *priv)
{
	/* 8:4mA(default), a:8mA, c:12mA, e:16mA */
#if defined(MTKAIFV4_SUPPORT)
	regmap_write(priv->regmap, MT6338_DRV_CON1, 0x88);
	regmap_write(priv->regmap, MT6338_DRV_CON2, 0x88);
#else
	regmap_update_bits(priv->regmap, MT6338_DRV_CON3, 0xff, 0x88);
	regmap_update_bits(priv->regmap, MT6338_DRV_CON4, 0xf, 0x8);
#endif
}

int mt6338_set_mtkaif_protocol(struct snd_soc_component *cmpnt,
			       int mtkaif_protocol)
{
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	priv->mtkaif_protocol = mtkaif_protocol;
	return 0;
}
EXPORT_SYMBOL_GPL(mt6338_set_mtkaif_protocol);

static void mt6338_set_playback_gpio(struct mt6338_priv *priv)
{
	regmap_write(priv->regmap, MT6338_GPIO_PULLEN0_SET, 0x30);
#if defined(MTKAIFV4_SUPPORT)

	/* set gpio mosi mode, clk / data mosi */
	/*
	 * [3:0] 1: AUD_DAT_MOSI0 (I) 2: TDMIN_DATA0 (I)
	 * [6:4] 1: AUD_DAT_MOSI1 (I) 2: TDMIN_DATA1 (I)
	 */
	regmap_write(priv->regmap, MT6338_GPIO_MODE2_SET, 0x11);
	/*
	 * [3:0] 1: AUD_DAT_MISO0 (O)2: TDMOUT_DATA0 (O)
	 * [6:4] 1: AUD_CLK_MOSI (I) 2: TDMOUT_BCK (I)
	 */
	regmap_write(priv->regmap, MT6338_GPIO_MODE3_SET, 0x10);
#else
	/* set gpio mosi mode, clk / data mosi */
	regmap_write(priv->regmap, MT6338_GPIO_MODE2_CLR, 0xff);
	regmap_write(priv->regmap, MT6338_GPIO_MODE2_SET, 0x22);
	/*
	 * [3:0] 1: TDMOUT_LRCK (I) 2: AUD_CLK_MISO (O)
	 * [6:4] 1: TDMIN_BCK (I)	 2: AUD_DAT_MISO1 (O)
	 */
	regmap_write(priv->regmap, MT6338_GPIO_MODE4_CLR, 0xf0);
	regmap_write(priv->regmap, MT6338_GPIO_MODE4_SET, 0x10);

	/*
	 * [3:0] 1: TDMIN_LRCK (I)
	 */
	regmap_write(priv->regmap, MT6338_GPIO_MODE5_CLR, 0xf);
	regmap_write(priv->regmap, MT6338_GPIO_MODE5_SET, 0x1);
#endif
}

static void mt6338_reset_playback_gpio(struct mt6338_priv *priv)
{
	regmap_write(priv->regmap, MT6338_GPIO_PULLEN0_CLR, 0x30);
	/* set pad_aud_*_mosi to GPIO mode and dir input
	 * reason:
	 * pad_aud_dat_mosi*, because the pin is used as boot strap
	 * don't clean clk/sync, for mtkaif protocol 2
	 */
	regmap_write(priv->regmap, MT6338_GPIO_MODE2_CLR, 0xff);
	/*
	 * [4]: PAD_AUD_DAT_MOSI1 [5]: PAD_AUD_DAT_MOSI0 [6]: PAD_AUD_DAT_MISO0
	 * [7]: PAD_AUD_CLK_MOSI
	 */

}

static void mt6338_set_capture_gpio(struct mt6338_priv *priv)
{
	/* set gpio miso mode */
#if defined(MTKAIFV4_SUPPORT)
	/*
	 * [3:0] 1: AUD_DAT_MISO0 (O)	2: TDMOUT_DATA0 (O)
	 * [6:4] 1: AUD_CLK_MOSI (I) 2: TDMOUT_BCK (I)
	 */
	regmap_write(priv->regmap, MT6338_GPIO_MODE3_SET, 0x11);
#else
	regmap_write(priv->regmap, MT6338_GPIO_MODE3_SET, 0x22);
	/*
	 * [3:0] 1: TDMOUT_LRCK (I) 2: AUD_CLK_MISO (O)
	 * [6:4] 1: TDMIN_BCK (I) 2: AUD_DAT_MISO1 (O)
	 */
	regmap_write(priv->regmap, MT6338_GPIO_MODE4_CLR, 0xf);
	regmap_write(priv->regmap, MT6338_GPIO_MODE4_SET, 0x1);
#endif
}

static void mt6338_reset_capture_gpio(struct mt6338_priv *priv)
{
	/* set pad_aud_*_miso to GPIO mode and dir input
	 * reason:
	 * pad_aud_clk_miso, because when playback only the miso_clk
	 * will also have 26m, so will have power leak
	 * pad_aud_dat_miso*, because the pin is used as boot strap
	 */

#if defined(MTKAIFV4_SUPPORT)
	regmap_write(priv->regmap, MT6338_GPIO_MODE3_CLR, 0xf);
#else
	regmap_write(priv->regmap, MT6338_GPIO_MODE3_CLR, 0xff);
	regmap_write(priv->regmap, MT6338_GPIO_MODE4_CLR, 0xf0);
	regmap_write(priv->regmap, MT6338_GPIO_MODE4_SET, 0x20);
#endif

}

static void  mt6338_set_vow_gpio(struct mt6338_priv *priv)
{
	regmap_write(priv->regmap, MT6338_GPIO_PULLEN1_SET, 0x1);

	/* vow gpio set (data) */
	/* [3:0] 4: VOW_DAT_MISO (O) */
	regmap_write(priv->regmap, MT6338_GPIO_MODE3_CLR, 0xff);
	regmap_write(priv->regmap, MT6338_GPIO_MODE3_SET, 0x14);

	/* vow gpio set (clock) */
	/* [3:0] 4: VOW_CLK_MISO (O)) */
	regmap_write(priv->regmap, MT6338_GPIO_MODE4_CLR, 0xf);
	regmap_write(priv->regmap, MT6338_GPIO_MODE4_SET, 0x4);
}

static void mt6338_reset_vow_gpio(struct mt6338_priv *priv)
{
	/* vow gpio clear (data) */
	regmap_write(priv->regmap, MT6338_GPIO_PULLEN1_CLR, 0x1);

	/* [3:0] 4: VOW_DAT_MISO (O) */
	regmap_write(priv->regmap, MT6338_GPIO_MODE3_CLR, 0xf);

	/* vow gpio clear (clock) */
	/* [3:0] 4: VOW_CLK_MISO (O)) */
	regmap_write(priv->regmap, MT6338_GPIO_MODE4_CLR, 0xf);
}

/* use only when not govern by DAPM */
static void mt6338_set_dcxo(struct mt6338_priv *priv, bool enable)
{
#if IS_ENABLED(CONFIG_MT6685_AUDCLK)
	dev_dbg(priv->dev, "%s() enable = %d\n",
			 __func__, enable);
	mt6685_set_dcxo(enable);
#endif
}

/* use only when doing mtkaif calibraiton at the boot time */
static void mt6338_set_clksq(struct mt6338_priv *priv, bool enable)
{
	/* Enable/disable CLKSQ 26MHz */
	regmap_write(priv->regmap, MT6338_CLKSQ_PMU_CON0,
		(enable ? 0xe : 0x0));
}
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
/* use only when doing mtkaif calibraiton at the boot time */
static void mt6338_set_pmu(struct mt6338_priv *priv, bool enable)
{
	if (enable) {
		/* VPLL18 LDO */
		regmap_update_bits(priv->regmap, MT6338_VPLL18_PMU_CON0,
			RG_VPLL18_LDO_PLL208M_EN_VA18_MASK_SFT,
			0x1 << RG_VPLL18_LDO_PLL208M_EN_VA18_SFT);

		/* PLL208M */
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON5,
			0x2);
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON4,
			0x17);
	} else {
		/* PLL208M */
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON4,
			0x16);
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON5,
			0x1);

		/* VPLL18 LDO */
		regmap_update_bits(priv->regmap, MT6338_VPLL18_PMU_CON0,
			RG_VPLL18_LDO_PLL208M_EN_VA18_MASK_SFT,
			0x0 << RG_VPLL18_LDO_PLL208M_EN_VA18_SFT);
	}
}
/* use only when doing mtkaif calibraiton at the boot time */
static void mt6338_set_aud_top(struct mt6338_priv *priv, bool enable)
{
	if (enable) {
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON1, 0x0);
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON2, 0x0);
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON3, 0x0);
	} else {
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON3, 0x2);
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON2, 0xff);
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON1, 0xbf);
	}
}
#endif
/* use only when doing mtkaif calibraiton at the boot time */
static void mt6338_set_aud_global_bias(struct mt6338_priv *priv, bool enable)
{
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON34,
		RG_AUDGLB_PWRDN_VA32_MASK_SFT,
		(enable ? 0 : 1) << RG_AUDGLB_PWRDN_VA32_SFT);
}

/* use only when doing mtkaif calibraiton at the boot time */
static void mt6338_set_topck(struct mt6338_priv *priv, bool enable)
{
	regmap_write(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0,
		(enable ? 0x0 : 0x66));
}

/* Digital Part */
static void mt6338_set_dl_src(struct mt6338_priv *priv, bool enable)
{
	unsigned int rate;

	if (priv->dl_rate[0] != 0)
		rate = mt6338_dlsrc_rate_transform(priv->dl_rate[0]);
	else
		rate = MT6338_DLSRC_48000HZ;

	if (enable) {
		/* Step1: Choose DL FS and DL enable and DL gain enable */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0_H,
			AFE_DL_INPUT_MODE_CTL_MASK_SFT,
			rate << AFE_DL_INPUT_MODE_CTL_SFT);
		/* nedd check D&A */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0_H,
			AFE_DL_CH1_SATURATION_EN_CTL_MASK_SFT |
			AFE_DL_CH2_SATURATION_EN_CTL_MASK_SFT,
			0x0 << AFE_DL_CH2_SATURATION_EN_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0_L,
			AFE_DL_MUTE_CH1_OFF_CTL_PRE_MASK_SFT |
			AFE_DL_MUTE_CH2_OFF_CTL_PRE_MASK_SFT,
			0x3 << AFE_DL_MUTE_CH2_OFF_CTL_PRE_SFT);
		/* nedd check D&A */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0,
			AFE_DL_VOICE_MODE_CTL_PRE_MASK_SFT,
			0x0 << AFE_DL_VOICE_MODE_CTL_PRE_SFT);
#ifdef ALIGN_SWING
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0,
			AFE_DL_GAIN_ON_CTL_PRE_MASK_SFT,
			0x1 << AFE_DL_GAIN_ON_CTL_PRE_SFT);
		/* Step2: DL digital gain control, 0xA028, (-4dB),	20*log(41000/2^16) */
		regmap_write(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON1_H, 0xa0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON1_M, 0x28);
#endif
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0,
			AFE_DL_SRC_ON_TMP_CTL_PRE_MASK_SFT,
			0x1 << AFE_DL_SRC_ON_TMP_CTL_PRE_SFT);

		/* MTKAIFV4 */
		if (priv->dl_rate[0] != 0)
			rate = mt6338_rate_transform(priv->dl_rate[0]);
		else
			rate = MT6338_ADDA_48000HZ;

		/* config mtkaif_rx1 (pmic), 2ch, */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0,
			MT6338_MTKAIFV4_RXIF_AFE_ON_MASK_SFT,
			0x1 << MT6338_MTKAIFV4_RXIF_AFE_ON_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0,
			MT6338_MTKAIFV4_RXIF_FOUR_CHANNEL_MASK_SFT,
			0x0 << MT6338_MTKAIFV4_RXIF_FOUR_CHANNEL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0,
			MT6338_MTKAIFV4_LOOPBACK1_MASK_SFT,
			0x0 << MT6338_MTKAIFV4_LOOPBACK1_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0,
			MT6338_MTKAIFV4_RXIF_INPUT_MODE_MASK_SFT,
			rate << MT6338_MTKAIFV4_RXIF_INPUT_MODE_SFT);
	} else {
#ifdef ALIGN_SWING
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0,
			AFE_DL_GAIN_ON_CTL_PRE_MASK_SFT,
			0x0 << AFE_DL_GAIN_ON_CTL_PRE_SFT);
#endif
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0,
			MT6338_MTKAIFV4_RXIF_AFE_ON_MASK_SFT,
			0x0 << MT6338_MTKAIFV4_RXIF_AFE_ON_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0,
			AFE_DL_SRC_ON_TMP_CTL_PRE_MASK_SFT,
			0x0 << AFE_DL_SRC_ON_TMP_CTL_PRE_SFT);
	}

}
static void mt6338_set_2nd_dl_src(struct mt6338_priv *priv, bool enable)
{
	unsigned int rate;

	if (priv->dl_rate[0] != 0)
		rate = mt6338_dlsrc_rate_transform(priv->dl_rate[0]);
	else
		rate = MT6338_DLSRC_48000HZ;

	/* 2ND DL */
	if (enable) {
		/* Step1: Choose DL FS and DL enable and DL gain enable */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0_H,
			AFE_2ND_DL_INPUT_MODE_CTL_MASK_SFT,
			rate << AFE_2ND_DL_INPUT_MODE_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0_H,
			AFE_2ND_DL_CH1_SATURATION_EN_CTL_MASK_SFT |
			AFE_2ND_DL_CH2_SATURATION_EN_CTL_MASK_SFT,
			0x3 << AFE_2ND_DL_CH2_SATURATION_EN_CTL_SFT);

		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0_L,
			AFE_2ND_DL_MUTE_CH1_OFF_CTL_PRE_MASK_SFT |
			AFE_2ND_DL_MUTE_CH2_OFF_CTL_PRE_MASK_SFT,
			0x3 << AFE_2ND_DL_MUTE_CH2_OFF_CTL_PRE_SFT);

		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0,
			AFE_2ND_DL_VOICE_MODE_CTL_PRE_MASK_SFT,
			0x0 << AFE_2ND_DL_VOICE_MODE_CTL_PRE_SFT);
#ifdef ALIGN_SWING
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0,
			AFE_2ND_DL_GAIN_ON_CTL_PRE_MASK_SFT,
			0x1 << AFE_2ND_DL_GAIN_ON_CTL_PRE_SFT);
		/* Step2: DL digital gain control, 0xA028, (-4dB),	20*log(41000/2^16) */
		regmap_write(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON1_H, 0xa0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON1_M, 0x28);
#endif

		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0,
			AFE_2ND_DL_SRC_ON_TMP_CTL_PRE_MASK_SFT,
			0x1 << AFE_2ND_DL_SRC_ON_TMP_CTL_PRE_SFT);

		/* MTKAIFV4 */
		if (priv->dl_rate[0] != 0)
			rate = mt6338_rate_transform(priv->dl_rate[0]);
		else
			rate = MT6338_ADDA_48000HZ;
		/* config mtkaif_rx2 (pmic), 2ch, */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0,
			MT6338_ADDA6_MTKAIFV4_RXIF_AFE_ON_MASK_SFT,
			0x1 << MT6338_ADDA6_MTKAIFV4_RXIF_AFE_ON_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0,
			MT6338_ADDA6_MTKAIFV4_RXIF_FOUR_CHANNEL_MASK_SFT,
			0x0 << MT6338_ADDA6_MTKAIFV4_RXIF_FOUR_CHANNEL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0,
			MT6338_ADDA6_MTKAIFV4_LOOPBACK1_MASK_SFT,
			0x0 << MT6338_ADDA6_MTKAIFV4_LOOPBACK1_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0,
			MT6338_ADDA6_MTKAIFV4_RXIF_INPUT_MODE_MASK_SFT,
			rate <<	MT6338_ADDA6_MTKAIFV4_RXIF_INPUT_MODE_SFT);

	} else {
#ifdef ALIGN_SWING
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0,
			AFE_2ND_DL_GAIN_ON_CTL_PRE_MASK_SFT,
			0x0 << AFE_2ND_DL_GAIN_ON_CTL_PRE_SFT);
#endif
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0,
			MT6338_ADDA6_MTKAIFV4_RXIF_AFE_ON_MASK_SFT,
			0x0 << MT6338_ADDA6_MTKAIFV4_RXIF_AFE_ON_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0,
			AFE_2ND_DL_SRC_ON_TMP_CTL_PRE_MASK_SFT,
			0x0 << AFE_2ND_DL_SRC_ON_TMP_CTL_PRE_SFT);
	}
}
static void mt6338_set_ulcf(struct mt6338_priv *priv, bool enable)
{
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_3,
		ADDA_ULCF_CFG_EN_CTL_MASK_SFT,
		enable << ADDA_ULCF_CFG_EN_CTL_SFT);
	if (enable) {
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_2, 0x9b);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_1, 0xff);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_0, 0x96);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_3, 0xff);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_2, 0x7a);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_0, 0x20);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_2, 0x6b);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_1, 0xff);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_0, 0xf0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_3, 0xff);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_2, 0xbc);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_0, 0xf);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_2, 0xb);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_1, 0xff);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_0, 0xe4);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_2, 0x43);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_0, 0x38);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_3, 0xff);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_2, 0x54);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_1, 0xff);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_0, 0x99);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_3, 0x1);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_2, 0x33);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_0, 0xac);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_3, 0xfe);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_2, 0x1d);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_1, 0xfe);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_0, 0xf2);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_3, 0x2);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_2, 0xc6);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_1, 0x1);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_0, 0x95);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_3, 0xfc);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_2, 0xa);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_1, 0xfd);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_0, 0xb2);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_3, 0x5);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_2, 0x9e);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_1, 0x3);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_0, 0x4b);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_3, 0xf7);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_2, 0xda);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_1, 0xfb);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_0, 0x4c);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_3, 0xc);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_2, 0xaf);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_1, 0x6);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_0, 0xd8);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_3, 0xe8);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_2, 0x65);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_1, 0xf5);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_0, 0x99);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_3, 0x49);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_2, 0x90);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_1, 0x10);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_0, 0x50);
	} else {
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_3, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_2, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_1, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_0, 0x0);
	}
}

static void mt6338_set_ul_src(struct mt6338_priv *priv, bool enable)
{
	unsigned int rate;

	if (priv->ul_rate[0] != 0)
		rate = mt6338_voice_rate_transform(priv->ul_rate[0]);
	else
		rate = MT6338_VOICE_48000HZ;

	if (enable) {
		/* Step3: UL mode setting: AMIC/DMIC mode, voice_mode, loopback_mode*/
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_2,
			MT6338_ADDA_UL_MODE_3P25M_CH2_CTL_MASK_SFT |
			MT6338_ADDA_UL_MODE_3P25M_CH1_CTL_MASK_SFT,
			0x0 << MT6338_ADDA_UL_MODE_3P25M_CH1_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_2,
			MT6338_ADDA_UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
			rate << MT6338_ADDA_UL_VOICE_MODE_CH1_CH2_CTL_SFT);

		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_1,
			ADDA_DMIC_LOW_POWER_MODE_CTL_MASK_SFT,
			0x0 << ADDA_DMIC_LOW_POWER_MODE_CTL_SFT);
		/* iir on */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_1,
			ADDA_UL_IIRMODE_CTL_MASK_SFT,
			0x0 << ADDA_UL_IIRMODE_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_1,
			ADDA_UL_IIR_ON_TMP_CTL_MASK_SFT,
			0x1 << ADDA_UL_IIR_ON_TMP_CTL_SFT);

		/* [0] ul_src_on = 1 */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0,
			ADDA_UL_SRC_ON_TMP_CTL_MASK_SFT,
			0x1 << ADDA_UL_SRC_ON_TMP_CTL_SFT);
	} else {
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0,
			ADDA_UL_SRC_ON_TMP_CTL_MASK_SFT,
			0x0 << ADDA_UL_SRC_ON_TMP_CTL_SFT);
	}
}

static void mt6338_set_ul34_src(struct mt6338_priv *priv, bool enable)
{
	unsigned int rate;

	if (priv->ul_rate[0] != 0)
		rate = mt6338_voice_rate_transform(priv->ul_rate[0]);
	else
		rate = MT6338_VOICE_48000HZ;

	/* ADDA6 */
	if (enable) {
		/* Step3: UL mode setting: AMIC/DMIC mode, voice_mode, loopback_mode*/
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_2,
			ADDA6_UL_MODE_3P25M_CH2_CTL_MASK_SFT |
			ADDA6_UL_MODE_3P25M_CH1_CTL_MASK_SFT,
			0x0 << ADDA6_UL_MODE_3P25M_CH1_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_2,
			ADDA6_UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
			rate << ADDA6_UL_VOICE_MODE_CH1_CH2_CTL_SFT);

		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_1,
			ADDA6_DMIC_LOW_POWER_MODE_CTL_MASK_SFT,
			0x0 << ADDA6_DMIC_LOW_POWER_MODE_CTL_SFT);
		/* iir on */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_1,
			ADDA6_UL_IIRMODE_CTL_MASK_SFT,
			0x0 << ADDA6_UL_IIRMODE_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_1,
			ADDA6_UL_IIR_ON_TMP_CTL_MASK_SFT,
			0x1 << ADDA6_UL_IIR_ON_TMP_CTL_SFT);

		/* [0] ul_src_on = 1 */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0,
			ADDA6_UL_SRC_ON_TMP_CTL_MASK_SFT,
			0x1 << ADDA6_UL_SRC_ON_TMP_CTL_SFT);

	} else {
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0,
			ADDA6_UL_SRC_ON_TMP_CTL_MASK_SFT,
			0x0 << ADDA6_UL_SRC_ON_TMP_CTL_SFT);
	}
}

/* Analog Part */
static void mt6338_set_decoder_clk(struct mt6338_priv *priv, bool enable, bool hifi)
{
	int mode = (hifi) ? 3 : 2;

	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON46,
			(enable ? mode : 0));
}

static void ldo_select_to_min(struct mt6338_priv *priv, bool enable, bool dual)
{
	unsigned int channel = 0x1;

	dev_dbg(priv->dev, "%s()\n", __func__);

	if (dual)
		channel = 0x3;

	if (enable) {
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON36,	0x0);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON39, 0x3f);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON37, 0x3f);
	} else {
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON37, 0x0);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x0);

	}
}

static void nvreg_select_to_min(struct mt6338_priv *priv, bool enable, bool dual)
{
	unsigned int channel = 0x1;

	if (dual)
		channel = 0x3;

	if (enable) {
		/* NVREG_DACSW vout selection */
		/* NVREG_HCBUF/NVREG_LCBUF vout selection */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON33,	0x0);
		/* Enable for NVREG */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON41, 0x3f);
	}
}

static void mt6338_mtkaif_tx_enable(struct mt6338_priv *priv)
{
	unsigned int rate;
	unsigned int value;

	if (priv->ul_rate[0] != 0)
		rate = mt6338_rate_transform(priv->ul_rate[0]);
	else
		rate = MT6338_ADDA_48000HZ;

	switch (priv->mtkaif_protocol) {
	case MT6338_MTKAIF_PROTOCOL_2_CLK_P2:
		/* enable aud_pad TX fifos */
		regmap_write(priv->regmap, MT6338_AFE_AUD_PAD_TOP, 0x39);
		/* MTKAIF TX format setting */
		regmap_update_bits(priv->regmap, MT6338_AFE_MTKAIFV4_TX_CFG,
			MT6338_MTKAIFV4_TXIF_PROTOCOL3_MASK_SFT,
			0x1 << MT6338_MTKAIFV4_TXIF_PROTOCOL3_SFT);
		/* config mtkaif_tx1 (pmic), 4ch */
		value = 0x1 << MT6338_MTKAIFV4_TXIF_AFE_ON_SFT |
				0x1 << MT6338_MTKAIFV4_TXIF_FOUR_CHANNEL_SFT |
				rate << MT6338_MTKAIFV4_TXIF_INPUT_MODE_SFT;
		regmap_write(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_TX_CFG0, value);
		/* config mtkaif_tx1 (pmic), 4ch */
		regmap_write(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_TX_CFG0, value);

		/* Set to VD105 = 1.5V */
		regmap_update_bits(priv->regmap, MT6338_STRUP_ELR_1,
			0x1f << 3, (priv->vd105 - 0xc) << 3);


		/* lpbk2 */
		/* regmap_update_bits(priv->regmap, MT6338_AFE_MTKAIFV4_TX_CFG,
		 * MT6338_MTKAIFV4_LOOPBACK2_MASK_SFT,
		 * 0x1 << MT6338_MTKAIFV4_LOOPBACK2_SFT);
		 */
		break;
	default:
		/* MTKAIF TX format setting */
		regmap_update_bits(priv->regmap, MT6338_AFE_MTKAIFV4_TX_CFG,
			MT6338_MTKAIFV4_TXIF_PROTOCOL3_MASK_SFT,
			0x0 << MT6338_MTKAIFV4_TXIF_PROTOCOL3_SFT);
		/* enable aud_pad TX fifos */
		regmap_write(priv->regmap, MT6338_AFE_AUD_PAD_TOP, 0x31);
		break;
	}

}

static void mt6338_mtkaif_tx_disable(struct mt6338_priv *priv)
{
	/* disable aud_pad TX fifos */
	regmap_write(priv->regmap, MT6338_AFE_AUD_PAD_TOP, 0x30);
}

void mt6338_mtkaif_calibration_enable(struct snd_soc_component *cmpnt)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	mt6338_set_playback_gpio(priv);
	mt6338_set_capture_gpio(priv);
	mt6338_mtkaif_tx_enable(priv);

	mt6338_set_dcxo(priv, true);
	mt6338_set_aud_global_bias(priv, true);
	mt6338_set_clksq(priv, true);
	mt6338_set_topck(priv, true);

	/* set dat_miso_loopback on */
	regmap_update_bits(priv->regmap, MT6338_AUDIO_DIG_CFG,
		RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK_SFT,
		0x1 << RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDIO_DIG_CFG_H,
		RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK_SFT,
		0x1 << RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDIO_DIG_CFG1,
		RG_AUD_PAD_TOP_DAT_MISO3_LOOPBACK_MASK_SFT,
		0x1 << RG_AUD_PAD_TOP_DAT_MISO3_LOOPBACK_SFT);
#endif
}
EXPORT_SYMBOL_GPL(mt6338_mtkaif_calibration_enable);

void mt6338_mtkaif_calibration_disable(struct snd_soc_component *cmpnt)
{
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	/* set dat_miso_loopback off */
	regmap_update_bits(priv->regmap, MT6338_AUDIO_DIG_CFG,
		RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_MASK_SFT,
		0x0 << RG_AUD_PAD_TOP_DAT_MISO_LOOPBACK_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDIO_DIG_CFG_H,
		RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_MASK_SFT,
		0x0 << RG_AUD_PAD_TOP_DAT_MISO2_LOOPBACK_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDIO_DIG_CFG1,
		RG_AUD_PAD_TOP_DAT_MISO3_LOOPBACK_MASK_SFT,
		0x0 << RG_AUD_PAD_TOP_DAT_MISO3_LOOPBACK_SFT);

	mt6338_set_topck(priv, false);
	mt6338_set_clksq(priv, false);
	mt6338_set_aud_global_bias(priv, false);
	mt6338_set_dcxo(priv, false);

	mt6338_mtkaif_tx_disable(priv);
	mt6338_reset_playback_gpio(priv);
	mt6338_reset_capture_gpio(priv);
}
EXPORT_SYMBOL_GPL(mt6338_mtkaif_calibration_disable);

void mt6338_set_mtkaif_calibration_phase(struct snd_soc_component *cmpnt,
					 int phase_1, int phase_2, int phase_3)
{
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	regmap_update_bits(priv->regmap, MT6338_AUDIO_DIG_CFG,
		RG_AUD_PAD_TOP_PHASE_MODE_MASK_SFT,
		phase_1 << RG_AUD_PAD_TOP_PHASE_MODE_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDIO_DIG_CFG_H,
		RG_AUD_PAD_TOP_PHASE_MODE2_MASK_SFT,
		phase_2 << RG_AUD_PAD_TOP_PHASE_MODE2_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDIO_DIG_CFG1,
		RG_AUD_PAD_TOP_PHASE_MODE3_MASK_SFT,
		phase_3 << RG_AUD_PAD_TOP_PHASE_MODE3_SFT);
}
EXPORT_SYMBOL_GPL(mt6338_set_mtkaif_calibration_phase);

static const char *const dl_pga_gain[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db",
	"3Db", "2Db", "1Db", "0Db", "-1Db",
	"-2Db", "-3Db",	"-4Db", "-5Db", "-6Db",
	"-7Db", "-8Db", "-9Db", "-10Db", "-40Db"
};

static const char *const hp_dl_pga_gain[] = {
	"9Db", "6Db", "3Db", "0Db"
};

static void hp_main_output_ramp(struct mt6338_priv *priv, bool up)
{
	int i = 0, stage = 0;
	int target = 7;

	/* Enable/Reduce HPL/R main output stage step by step */
	for (i = 0; i <= target; i++) {
		stage = up ? i : target - i;
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
			RG_DA_HP_OUTSTG_RCH_MASK_SFT,
			stage << RG_DA_HP_OUTSTG_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
			RG_DA_HP_OUTSTG_LCH_MASK_SFT,
			stage << RG_DA_HP_OUTSTG_LCH_SFT);
		if ((!priv->dc_trim.calibrated) || (priv->hp_hifi_mode == 2))
			usleep_range(600, 650);
		else
			usleep_range(100, 120);
	}
}

static void hp_ln_gain_ramp(struct mt6338_priv *priv, bool up)
{
	int i = 0, stage = 0;
	int target = 7;

	/*Set HP LN gain step by step */
	for (i = 2; i <= target; i++) {
		stage = up ? i : target + 1 - i;
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H,
			RG_DA_ANA_HP_LNGAIN_ATT_RCH_MASK_SFT,
			stage << RG_DA_ANA_HP_LNGAIN_ATT_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L,
			RG_DA_ANA_HP_LNGAIN_ATT_LCH_MASK_SFT,
			stage << RG_DA_ANA_HP_LNGAIN_ATT_LCH_SFT);
		if (priv->hp_hifi_mode == 2)
			usleep_range(600, 620);
		else
			usleep_range(100, 120);
	}
}

static void hp_aux_feedback_loop_gain_ramp(struct mt6338_priv *priv, bool up)
{
	int i = 0, stage = 0;
	int target = 0xf;
	int value = 0;

	/* Enable/Reduce HP aux feedback loop gain step by step */
	for (i = 0; i <= target; i++) {
		stage = up ? i : target - i;
		value = stage << RG_AUDHPLAUXGAIN_VAUDP18_SFT |
			    stage << RG_AUDHPRAUXGAIN_VAUDP18_SFT;
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON8, value);
		if (!priv->dc_trim.calibrated)
			usleep_range(600, 650);
		else
			usleep_range(600, 620);
	}
}

static void hp_in_pair_current(struct mt6338_priv *priv, bool increase)
{
	int i = 0, stage = 0;
	int target = 0x3;

	/* Reduce HP aux feedback loop gain step by step */
	for (i = 0; i <= target; i++) {
		stage = increase ? i : target - i;
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
			RG_AUDHPDIFFINPBIASADJ_VAUDP18_MASK_SFT,
			stage << RG_AUDHPDIFFINPBIASADJ_VAUDP18_SFT);
		if ((!priv->dc_trim.calibrated) || (priv->hp_hifi_mode == 2))
			usleep_range(600, 650);
		else
			usleep_range(100, 120);
	}
}

static void hp_pull_down(struct mt6338_priv *priv,
							 bool enable, bool increase)
{
	int i = 0, stage = 0;
	int target = 0x7;
	unsigned int value = 0;

	for (i = 0x0; i <= 0x7; i++) {
		stage = increase ? i : target - i;
		value = enable << RG_AUDHPTRIM_EN_VAUDP18_SFT |
				stage << RG_HPPSHORT2VCM_VAUDP18_SFT;
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON11, value);
		if ((!priv->dc_trim.calibrated) || (priv->hp_hifi_mode == 2))
			usleep_range(600, 650);
		else
			usleep_range(100, 120);
	}
}
#ifdef NLE_IMP
static int hp_gain_ctl_select(struct mt6338_priv *priv,
			      unsigned int hp_gain_ctl)
{
	if (hp_gain_ctl >= HP_GAIN_CTL_NUM) {
		dev_warn(priv->dev, "%s(), hp_gain_ctl %d invalid\n",
			 __func__, hp_gain_ctl);
		return -EINVAL;
	}

	priv->hp_gain_ctl = hp_gain_ctl;
	regmap_update_bits(priv->regmap, MT6338_AFE_DL_NLE_CFG_L,
		NLE_LCH_HPGAIN_SEL_MASK_SFT,
		hp_gain_ctl << NLE_LCH_HPGAIN_SEL_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_DL_NLE_CF_H,
		NLE_RCH_HPGAIN_SEL_MASK_SFT,
		hp_gain_ctl << NLE_RCH_HPGAIN_SEL_SFT);

	return 0;
}
#endif
static bool is_valid_hp_pga_idx(int reg_idx)
{
	return (reg_idx >= HP_GAIN_9DB && reg_idx <= HP_GAIN_0DB) ||
	       reg_idx == HP_GAIN_0DB;
}

static void headset_volume_ramp(struct mt6338_priv *priv,
				int from, int to)
{
	int offset = 0, count = 1, reg_idx;
	unsigned int value = 0;

	if (from < 0) {
		regmap_read(priv->regmap, MT6338_ZCD_CON2, &value);
		from = (int)value;
	}

	if (!is_valid_hp_pga_idx(from) || !is_valid_hp_pga_idx(to)) {
		dev_warn(priv->dev, "%s(), volume index is not valid, from %d, to %d\n",
			 __func__, from, to);
		return;
	}

	dev_dbg(priv->dev, "%s(), from %d, to %d\n",
		 __func__, from, to);

	if (to > from)
		offset = to - from;
	else
		offset = from - to;

	while (offset > 0) {
		if (to > from)
			reg_idx = from + count;
		else
			reg_idx = from - count;

		if (is_valid_hp_pga_idx(reg_idx)) {
			regmap_update_bits(priv->regmap, MT6338_ZCD_CON2,
				RG_AUDHPLGAIN_MASK_SFT,
				reg_idx << RG_AUDHPLGAIN_SFT);
			regmap_update_bits(priv->regmap, MT6338_ZCD_CON2_H,
				RG_AUDHPRGAIN_MASK_SFT,
				reg_idx << RG_AUDHPRGAIN_SFT);
		}
		offset--;
		count++;
	}
}

static int dmic_used_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	ucontrol->value.integer.value[0] =
		priv->mux_select[MUX_MIC_TYPE_0] == MIC_TYPE_MUX_DMIC ||
		priv->mux_select[MUX_MIC_TYPE_1] == MIC_TYPE_MUX_DMIC ||
		priv->mux_select[MUX_MIC_TYPE_2] == MIC_TYPE_MUX_DMIC;

	return 0;
}

static int mt6338_snd_soc_put_volsw(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int reg2 = mc->rreg;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	int min = mc->min;
	unsigned int sign_bit = mc->sign_bit;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	int err;
	bool type_2r = false;
	unsigned int val2 = 0;
	unsigned int val, val_mask;

	if (sign_bit)
		mask = BIT(sign_bit + 1) - 1;

	val = ucontrol->value.integer.value[0];
	val = (val + min) & mask;
	if (invert)
		val = max - val;
	val_mask = mask << shift;
	val = val << shift;
	if (snd_soc_volsw_is_stereo(mc)) {
		val2 = ucontrol->value.integer.value[1];
		val2 = (val2 + min) & mask;
		if (invert)
			val2 = max - val2;
		if (reg == reg2) {
			val_mask |= mask << rshift;
			val |= val2 << rshift;
		} else {
			val2 = val2 << shift;
			type_2r = true;
		}
	}
	err = snd_soc_component_update_bits(component, reg, val_mask, val);
	if (err < 0)
		return err;

	if (type_2r)
		err = snd_soc_component_update_bits(component, reg2, val_mask, val2);

	return err;
}

static int mt6338_put_volsw(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(component);
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = 0;
	int index = ucontrol->value.integer.value[0];
	int indexR = ucontrol->value.integer.value[1];
	int ret;

	dev_dbg(priv->dev,
		"%s(), name %s, reg(0x%x) = 0x%x, set index = %x indeR = %x\n",
		 __func__, kcontrol->id.name, mc->reg, reg, index, indexR);

	switch (mc->reg) {
	case MT6338_ZCD_CON2:
	case MT6338_ZCD_CON2_H:
		if (priv->hp_hifi_mode) {
			ucontrol->value.integer.value[0] = 2;
			ucontrol->value.integer.value[1] = 2;
		} else {
			ucontrol->value.integer.value[0] = 1;
			ucontrol->value.integer.value[1] = 1;
		}
		break;
	case MT6338_AUDENC_PMU_CON1:
		if (priv->mic_hifi_mode)
			ucontrol->value.integer.value[0] = index + 2;
		break;
	case MT6338_AUDENC_PMU_CON3:
		if (priv->mic_hifi_mode)
			ucontrol->value.integer.value[0] = index + 2;
		break;
	case MT6338_AUDENC_PMU_CON5:
		if (priv->mic_hifi_mode)
			ucontrol->value.integer.value[0] = index + 2;
		break;
	case MT6338_AUDENC_PMU_CON7:
		if (priv->mic_hifi_mode)
			ucontrol->value.integer.value[0] = index + 2;
		break;
	}

	ret = mt6338_snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret < 0)
		return ret;

	switch (mc->reg) {
	case MT6338_ZCD_CON2:
	case MT6338_ZCD_CON2_H:
		regmap_read(priv->regmap, MT6338_ZCD_CON2, &reg);
		priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL] =
			(reg >> RG_AUDHPLGAIN_SFT) & RG_AUDHPLGAIN_MASK;
		regmap_read(priv->regmap, MT6338_ZCD_CON2_H, &reg);
		priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTR] =
			(reg >> RG_AUDHPRGAIN_SFT) & RG_AUDHPRGAIN_MASK;
		break;
	case MT6338_ZCD_CON1:
	case MT6338_ZCD_CON1_H:
		regmap_read(priv->regmap, MT6338_ZCD_CON1, &reg);
		priv->ana_gain[AUDIO_ANALOG_VOLUME_LINEOUTL] =
			(reg >> RG_AUDLOLGAIN_SFT) & RG_AUDLOLGAIN_MASK;
		regmap_read(priv->regmap, MT6338_ZCD_CON1_H, &reg);
		priv->ana_gain[AUDIO_ANALOG_VOLUME_LINEOUTR] =
			(reg >> RG_AUDLORGAIN_SFT) & RG_AUDLORGAIN_MASK;
		break;
	case MT6338_ZCD_CON3:
		regmap_read(priv->regmap, MT6338_ZCD_CON3, &reg);
		priv->ana_gain[AUDIO_ANALOG_VOLUME_HSOUTL] =
			(reg >> RG_AUDHSGAIN_SFT) & RG_AUDHSGAIN_MASK;
		break;
	case MT6338_AUDENC_PMU_CON1:
		regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON1, &reg);
		priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP1] =
			(reg >> RG_AUDPREAMPLGAIN_SFT) & RG_AUDPREAMPLGAIN_MASK;
		break;
	case MT6338_AUDENC_PMU_CON3:
		regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON3, &reg);
		priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP2] =
			(reg >> RG_AUDPREAMPRGAIN_SFT) & RG_AUDPREAMPRGAIN_MASK;
		break;
	case MT6338_AUDENC_PMU_CON5:
		regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON5, &reg);
		priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP3] =
			(reg >> RG_AUDPREAMP3GAIN_SFT) & RG_AUDPREAMP3GAIN_MASK;
		break;
	case MT6338_AUDENC_PMU_CON7:
		regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON7, &reg);
		priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP4] =
			(reg >> RG_AUDPREAMP4GAIN_SFT) & RG_AUDPREAMP4GAIN_MASK;
		break;
	case MT6338_AUDENC_PMU_CON10:
		regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON10, &reg);
		priv->ana_gain[AUDIO_ANALOG_NEG_VOLUME_MICAMP1] =
			(reg >> RG_AUDPREAMPLNEGGAIN_SFT) & RG_AUDPREAMPLNEGGAIN_MASK;
		priv->ana_gain[AUDIO_ANALOG_NEG_VOLUME_MICAMP2] =
			(reg >> RG_AUDPREAMPRNEGGAIN_SFT) & RG_AUDPREAMPRNEGGAIN_MASK;
		priv->ana_gain[AUDIO_ANALOG_NEG_VOLUME_MICAMP3] =
			(reg >> RG_AUDPREAMP3NEGGAIN_SFT) & RG_AUDPREAMP3NEGGAIN_MASK;
		priv->ana_gain[AUDIO_ANALOG_NEG_VOLUME_MICAMP4] =
			(reg >> RG_AUDPREAMP4NEGGAIN_SFT) & RG_AUDPREAMP4NEGGAIN_MASK;
		break;
	}

	dev_info(priv->dev, "%s(), name %s, reg(0x%x) = 0x%x, set index = %x\n",
		 __func__, kcontrol->id.name, mc->reg, reg, index);

	return ret;
}

static const DECLARE_TLV_DB_SCALE(hp_playback_tlv, 0, 300, 0);
static const DECLARE_TLV_DB_SCALE(playback_tlv, -1000, 100, 0);
static const DECLARE_TLV_DB_SCALE(capture_tlv, 0, 360, 0);
static const DECLARE_TLV_DB_SCALE(capture_neg_tlv, -900, 300, 0);

#define MT_SOC_ENUM_EXT_ID(xname, xenum, xhandler_get, xhandler_put, id) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .device = id,\
	.info = snd_soc_info_enum_double, \
	.get = xhandler_get, .put = xhandler_put, \
	.private_value = (unsigned long)&xenum \
}

static const struct snd_kcontrol_new mt6338_snd_controls[] = {
	/* dl pga gain */
	SOC_DOUBLE_R_EXT_TLV("Headset Volume",
			   MT6338_ZCD_CON2, MT6338_ZCD_CON2_H,
			   0, 0x3, 0,
			   snd_soc_get_volsw, mt6338_put_volsw,
			   hp_playback_tlv),
	SOC_DOUBLE_R_EXT_TLV("Lineout Volume",
			   MT6338_ZCD_CON1, MT6338_ZCD_CON1_H,
			   0, 0x12, 0,
			   snd_soc_get_volsw, mt6338_put_volsw, playback_tlv),
	SOC_SINGLE_EXT_TLV("Handset Volume",
			   MT6338_ZCD_CON3, 0, 0x12, 0,
			   snd_soc_get_volsw, mt6338_put_volsw, playback_tlv),

	/* ul pga gain */
	SOC_SINGLE_EXT_TLV("PGA1 Volume",
			   MT6338_AUDENC_PMU_CON1, RG_AUDPREAMPLGAIN_SFT, 0xa, 0,
			   snd_soc_get_volsw, mt6338_put_volsw, capture_tlv),
	SOC_SINGLE_EXT_TLV("PGA2 Volume",
			   MT6338_AUDENC_PMU_CON3, RG_AUDPREAMPRGAIN_SFT, 0xa, 0,
			   snd_soc_get_volsw, mt6338_put_volsw, capture_tlv),
	SOC_SINGLE_EXT_TLV("PGA3 Volume",
			   MT6338_AUDENC_PMU_CON5, RG_AUDPREAMP3GAIN_SFT, 0xa, 0,
			   snd_soc_get_volsw, mt6338_put_volsw, capture_tlv),
	SOC_SINGLE_EXT_TLV("PGA4 Volume",
			   MT6338_AUDENC_PMU_CON7, RG_AUDPREAMP4GAIN_SFT, 0xa, 0,
			   snd_soc_get_volsw, mt6338_put_volsw, capture_tlv),

	/* ul pga neg gain */
	SOC_SINGLE_EXT_TLV("NEG_PGA1 Volume",
			   MT6338_AUDENC_PMU_CON10, RG_AUDPREAMPLNEGGAIN_SFT, 0x3, 1,
			   snd_soc_get_volsw, mt6338_put_volsw, capture_neg_tlv),
	SOC_SINGLE_EXT_TLV("NEG_PGA2 Volume",
			   MT6338_AUDENC_PMU_CON10, RG_AUDPREAMPRNEGGAIN_SFT, 0x3, 1,
			   snd_soc_get_volsw, mt6338_put_volsw, capture_neg_tlv),
	SOC_SINGLE_EXT_TLV("NEG_PGA3 Volume",
			   MT6338_AUDENC_PMU_CON10, RG_AUDPREAMP3NEGGAIN_SFT, 0x3, 1,
			   snd_soc_get_volsw, mt6338_put_volsw, capture_neg_tlv),
	SOC_SINGLE_EXT_TLV("NEG_PGA4 Volume",
			   MT6338_AUDENC_PMU_CON10, RG_AUDPREAMP4NEGGAIN_SFT, 0x3, 1,
			   snd_soc_get_volsw, mt6338_put_volsw, capture_neg_tlv),

	/* debug */
	SOC_SINGLE_EXT("Codec keylock", SND_SOC_NOPM, 0, 0x1, 0,
			   mt6338_key_get, mt6338_key_set),

};

/* LOL MUX */
static const char * const lo_in_mux_map[] = {
	"Open", "Playback_L_DAC", "Playback", "Test Mode"
};

static int lo_in_mux_map_value[] = {
	0x0, 0x1, 0x2, 0x3,
};

static SOC_VALUE_ENUM_SINGLE_DECL(lo_in_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  LO_MUX_MASK,
				  lo_in_mux_map,
				  lo_in_mux_map_value);

static const struct snd_kcontrol_new lo_in_mux_control =
	SOC_DAPM_ENUM("LO Select", lo_in_mux_map_enum);

/*HP MUX */
static const char * const hp_in_mux_map[] = {
	"Open",
	"LoudSPK Playback",
	"Audio Playback",
	"Test Mode",
	"HP Impedance",
};

static int hp_in_mux_map_value[] = {
	HP_MUX_OPEN,
	HP_MUX_HPSPK,
	HP_MUX_HP,
	HP_MUX_TEST_MODE,
	HP_MUX_HP_IMPEDANCE,
};

static SOC_VALUE_ENUM_SINGLE_DECL(hpl_in_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  HP_MUX_MASK,
				  hp_in_mux_map,
				  hp_in_mux_map_value);

static const struct snd_kcontrol_new hpl_in_mux_control =
	SOC_DAPM_ENUM("HPL Select", hpl_in_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(hpr_in_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  HP_MUX_MASK,
				  hp_in_mux_map,
				  hp_in_mux_map_value);

static const struct snd_kcontrol_new hpr_in_mux_control =
	SOC_DAPM_ENUM("HPR Select", hpr_in_mux_map_enum);

/* RCV MUX */
static const char * const rcv_in_mux_map[] = {
	"Open", "Mute", "Voice Playback", "Test Mode"
};

static int rcv_in_mux_map_value[] = {
	RCV_MUX_OPEN,
	RCV_MUX_MUTE,
	RCV_MUX_VOICE_PLAYBACK,
	RCV_MUX_TEST_MODE,
};

static SOC_VALUE_ENUM_SINGLE_DECL(rcv_in_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  RCV_MUX_MASK,
				  rcv_in_mux_map,
				  rcv_in_mux_map_value);

static const struct snd_kcontrol_new rcv_in_mux_control =
	SOC_DAPM_ENUM("RCV Select", rcv_in_mux_map_enum);

/* DAC In MUX */
static const char * const dac_in_mux_map[] = {
	"Normal Path", "Sgen"
};

static int dac_in_mux_map_value[] = {
	0x0, 0x1,
};

static SOC_VALUE_ENUM_SINGLE_DECL(dac_in_mux_map_enum,
				  MT6338_AFE_TOP_CON0,
				  DL_SINE_ON_SFT,
				  DL_SINE_ON_MASK,
				  dac_in_mux_map,
				  dac_in_mux_map_value);

static const struct snd_kcontrol_new dac_in_mux_control =
	SOC_DAPM_ENUM("DAC Select", dac_in_mux_map_enum);

/* AIF Out MUX */
static SOC_VALUE_ENUM_SINGLE_DECL(aif_out_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  0,
				  dac_in_mux_map,
				  dac_in_mux_map_value);

static const struct snd_kcontrol_new aif_out_mux_control =
	SOC_DAPM_ENUM("AIF Out Select", aif_out_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(aif2_out_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  0,
				  dac_in_mux_map,
				  dac_in_mux_map_value);

static const struct snd_kcontrol_new aif2_out_mux_control =
	SOC_DAPM_ENUM("AIF Out Select", aif2_out_mux_map_enum);

/* UL SRC MUX */
static const char * const ul_src_mux_map[] = {
	"AMIC",
	"DMIC",
};

static int ul_src_mux_map_value[] = {
	UL_SRC_MUX_AMIC,
	UL_SRC_MUX_DMIC,
};

static SOC_VALUE_ENUM_SINGLE_DECL(ul_src_mux_map_enum,
				  MT6338_AFE_ADDA_UL_SRC_CON0_0,
				  ADDA_UL_SDM_3_LEVEL_CTL_SFT,
				  ADDA_UL_SDM_3_LEVEL_CTL_MASK,
				  ul_src_mux_map,
				  ul_src_mux_map_value);


static const struct snd_kcontrol_new ul_src_mux_control =
	SOC_DAPM_ENUM("UL_SRC_MUX Select", ul_src_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(ul2_src_mux_map_enum,
				  MT6338_AFE_ADDA6_UL_SRC_CON0_0,
				  ADDA6_UL_SDM_3_LEVEL_CTL_SFT,
				  ADDA6_UL_SDM_3_LEVEL_CTL_MASK,
				  ul_src_mux_map,
				  ul_src_mux_map_value);

static const struct snd_kcontrol_new ul2_src_mux_control =
	SOC_DAPM_ENUM("UL_SRC_MUX Select", ul2_src_mux_map_enum);

#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
/* VOW UL SRC MUX */
static SOC_VALUE_ENUM_SINGLE_DECL(vow_ul_src_mux_map_enum,
				  MT6338_AFE_VOW_TOP_CON0,
				  VOW_SDM_3_LEVEL_SFT,
				  VOW_SDM_3_LEVEL_MASK,
				  ul_src_mux_map,
				  ul_src_mux_map_value);

static const struct snd_kcontrol_new vow_ul_src_mux_control =
	SOC_DAPM_ENUM("VOW_UL_SRC_MUX Select", vow_ul_src_mux_map_enum);
#endif
/* MISO MUX */
static const char * const miso_mux_map[] = {
	"UL1_CH1",
	"UL1_CH2",
	"UL2_CH1",
	"UL2_CH2",
};

static int miso_mux_map_value[] = {
	MISO_MUX_UL1_CH1,
	MISO_MUX_UL1_CH2,
	MISO_MUX_UL2_CH1,
	MISO_MUX_UL2_CH2,
};

static SOC_VALUE_ENUM_SINGLE_DECL(miso0_mux_map_enum,
				  MT6338_AFE_MTKAIF_MUX_CFG,
				  RG_ADDA_CH1_SEL_SFT,
				  RG_ADDA_CH1_SEL_MASK,
				  miso_mux_map,
				  miso_mux_map_value);

static const struct snd_kcontrol_new miso0_mux_control =
	SOC_DAPM_ENUM("MISO_MUX Select", miso0_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(miso1_mux_map_enum,
				  MT6338_AFE_MTKAIF_MUX_CFG,
				  RG_ADDA_CH2_SEL_SFT,
				  RG_ADDA_CH2_SEL_MASK,
				  miso_mux_map,
				  miso_mux_map_value);

static const struct snd_kcontrol_new miso1_mux_control =
	SOC_DAPM_ENUM("MISO_MUX Select", miso1_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(miso2_mux_map_enum,
				  MT6338_AFE_MTKAIF_MUX_CFG_H,
				  RG_ADDA6_CH1_SEL_SFT,
				  RG_ADDA6_CH1_SEL_MASK,
				  miso_mux_map,
				  miso_mux_map_value);

static const struct snd_kcontrol_new miso2_mux_control =
	SOC_DAPM_ENUM("MISO_MUX Select", miso2_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(miso3_mux_map_enum,
				  MT6338_AFE_MTKAIF_MUX_CFG_H,
				  RG_ADDA6_CH2_SEL_SFT,
				  RG_ADDA6_CH2_SEL_MASK,
				  miso_mux_map,
				  miso_mux_map_value);

static const struct snd_kcontrol_new miso3_mux_control =
	SOC_DAPM_ENUM("MIS0_MUX Select", miso3_mux_map_enum);

#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
/* VOW AMIC MUX */
static const char * const vow_amic_mux_map[] = {
	"ADC_DATA_0",
	"ADC_DATA_1",
	"ADC_DATA_2",
	"ADC_DATA_3"
};

static int vow_amic_mux_map_value[] = {
	VOW_AMIC_MUX_ADC_DATA_0,
	VOW_AMIC_MUX_ADC_DATA_1,
	VOW_AMIC_MUX_ADC_DATA_2,
	VOW_AMIC_MUX_ADC_DATA_3
};

/* VOW AMIC MUX */
static SOC_VALUE_ENUM_SINGLE_DECL(vow_amic0_mux_map_enum,
				  MT6338_AFE_AMIC_ARRAY_CFG,
				  RG_AMIC_ADC0_SOURCE_SEL_SFT,
				  RG_AMIC_ADC0_SOURCE_SEL_MASK,
				  vow_amic_mux_map,
				  vow_amic_mux_map_value);

static const struct snd_kcontrol_new vow_amic0_mux_control =
	SOC_DAPM_ENUM("VOW_AMIC_MUX Select", vow_amic0_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(vow_amic1_mux_map_enum,
				  MT6338_AFE_AMIC_ARRAY_CFG,
				  RG_AMIC_ADC1_SOURCE_SEL_SFT,
				  RG_AMIC_ADC1_SOURCE_SEL_MASK,
				  vow_amic_mux_map,
				  vow_amic_mux_map_value);

static const struct snd_kcontrol_new vow_amic1_mux_control =
	SOC_DAPM_ENUM("VOW_AMIC_MUX Select", vow_amic1_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(vow_amic2_mux_map_enum,
				  MT6338_AFE_AMIC_ARRAY_CFG,
				  RG_AMIC_ADC2_SOURCE_SEL_SFT,
				  RG_AMIC_ADC2_SOURCE_SEL_MASK,
				  vow_amic_mux_map,
				  vow_amic_mux_map_value);

static const struct snd_kcontrol_new vow_amic2_mux_control =
	SOC_DAPM_ENUM("VOW_AMIC_MUX Select", vow_amic2_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(vow_amic3_mux_map_enum,
				  MT6338_AFE_AMIC_ARRAY_CFG,
				  RG_AMIC_ADC3_SOURCE_SEL_SFT,
				  RG_AMIC_ADC3_SOURCE_SEL_MASK,
				  vow_amic_mux_map,
				  vow_amic_mux_map_value);

static const struct snd_kcontrol_new vow_amic3_mux_control =
	SOC_DAPM_ENUM("VOW_AMIC_MUX Select", vow_amic3_mux_map_enum);
#endif
/* DMIC MUX */
static const char * const dmic_mux_map[] = {
	"DMIC_DATA0",
	"DMIC_DATA1",
	"DMIC_DATA2",
	"DMIC_DATA3",
};

static int dmic_mux_map_value[] = {
	DMIC_MUX_DMIC_DATA0,
	DMIC_MUX_DMIC_DATA1,
	DMIC_MUX_DMIC_DATA2,
	DMIC_MUX_DMIC_DATA3,
};

static SOC_VALUE_ENUM_SINGLE_DECL(dmic0_mux_map_enum,
				  MT6338_AO_AFE_DMIC_ARRAY_CFG,
				  RG_DMIC_ADC0_SOURCE_SEL_SFT,
				  RG_DMIC_ADC0_SOURCE_SEL_MASK,
				  dmic_mux_map,
				  dmic_mux_map_value);

static const struct snd_kcontrol_new dmic0_mux_control =
	SOC_DAPM_ENUM("DMIC_MUX Select", dmic0_mux_map_enum);

/* ul2 ch1 use RG_DMIC_ADC2_SOURCE_SEL */
static SOC_VALUE_ENUM_SINGLE_DECL(dmic1_mux_map_enum,
				  MT6338_AO_AFE_DMIC_ARRAY_CFG,
				  RG_DMIC_ADC1_SOURCE_SEL_SFT,
				  RG_DMIC_ADC1_SOURCE_SEL_MASK,
				  dmic_mux_map,
				  dmic_mux_map_value);

static const struct snd_kcontrol_new dmic1_mux_control =
	SOC_DAPM_ENUM("DMIC_MUX Select", dmic1_mux_map_enum);

/* ul1 ch2 use RG_DMIC_ADC3_SOURCE_SEL */
static SOC_VALUE_ENUM_SINGLE_DECL(dmic2_mux_map_enum,
				  MT6338_AO_AFE_DMIC_ARRAY_CFG,
				  RG_DMIC_ADC2_SOURCE_SEL_SFT,
				  RG_DMIC_ADC2_SOURCE_SEL_MASK,
				  dmic_mux_map,
				  dmic_mux_map_value);

static const struct snd_kcontrol_new dmic2_mux_control =
	SOC_DAPM_ENUM("DMIC_MUX Select", dmic2_mux_map_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(dmic3_mux_map_enum,
				  MT6338_AO_AFE_DMIC_ARRAY_CFG,
				  RG_DMIC_ADC3_SOURCE_SEL_SFT,
				  RG_DMIC_ADC3_SOURCE_SEL_MASK,
				  dmic_mux_map,
				  dmic_mux_map_value);

static const struct snd_kcontrol_new dmic3_mux_control =
	SOC_DAPM_ENUM("DMIC_MUX Select", dmic3_mux_map_enum);

/* ADC L MUX */
static const char * const adc_left_mux_map[] = {
	"Idle", "AIN0", "Left Preamplifier", "Idle_1"
};

static int adc_mux_map_value[] = {
	ADC_MUX_IDLE,
	ADC_MUX_AIN0,
	ADC_MUX_PREAMPLIFIER,
	ADC_MUX_IDLE1,
};

static SOC_VALUE_ENUM_SINGLE_DECL(adc_left_mux_map_enum,
				  MT6338_AUDENC_PMU_CON1,
				  RG_AUDADCLINPUTSEL_SFT,
				  RG_AUDADCLINPUTSEL_MASK,
				  adc_left_mux_map,
				  adc_mux_map_value);

static const struct snd_kcontrol_new adc_left_mux_control =
	SOC_DAPM_ENUM("ADC L Select", adc_left_mux_map_enum);

/* ADC R MUX */
static const char * const adc_right_mux_map[] = {
	"Idle", "AIN0", "Right Preamplifier", "Idle_1"
};

static SOC_VALUE_ENUM_SINGLE_DECL(adc_right_mux_map_enum,
				  MT6338_AUDENC_PMU_CON3,
				  RG_AUDADCRINPUTSEL_SFT,
				  RG_AUDADCRINPUTSEL_MASK,
				  adc_right_mux_map,
				  adc_mux_map_value);

static const struct snd_kcontrol_new adc_right_mux_control =
	SOC_DAPM_ENUM("ADC R Select", adc_right_mux_map_enum);

/* ADC 3 MUX */
static const char * const adc_mux_map[] = {
	"Idle", "AIN0", "Preamplifier", "Idle_1"
};

static SOC_VALUE_ENUM_SINGLE_DECL(adc_3_mux_map_enum,
				  MT6338_AUDENC_PMU_CON5,
				  RG_AUDADC3INPUTSEL_SFT,
				  RG_AUDADC3INPUTSEL_MASK,
				  adc_mux_map,
				  adc_mux_map_value);

static const struct snd_kcontrol_new adc_3_mux_control =
	SOC_DAPM_ENUM("ADC 3 Select", adc_3_mux_map_enum);

/* ADC 4 MUX */
static SOC_VALUE_ENUM_SINGLE_DECL(adc_4_mux_map_enum,
				  MT6338_AUDENC_PMU_CON7,
				  RG_AUDADC4INPUTSEL_SFT,
				  RG_AUDADC4INPUTSEL_MASK,
				  adc_mux_map,
				  adc_mux_map_value);

static const struct snd_kcontrol_new adc_4_mux_control =
	SOC_DAPM_ENUM("ADC 4 Select", adc_4_mux_map_enum);

/* PGA L MUX */
static const char * const pga_mux_map[] = {
	 "AIN0", "AIN1", "AIN2", "None",
};

static int pga_mux_map_value[] = {
	PGA_MUX_AIN0,
	PGA_MUX_AIN1,
	PGA_MUX_AIN2,
	PGA_MUX_NONE
};

static SOC_VALUE_ENUM_SINGLE_DECL(pga_left_mux_map_enum,
				  MT6338_AUDENC_PMU_CON0,
				  RG_AUDPREAMPLINPUTSEL_SFT,
				  RG_AUDPREAMPLINPUTSEL_MASK,
				  pga_mux_map,
				  pga_mux_map_value);

static const struct snd_kcontrol_new pga_left_mux_control =
	SOC_DAPM_ENUM("PGA L Select", pga_left_mux_map_enum);

/* PGA R MUX */
static SOC_VALUE_ENUM_SINGLE_DECL(pga_right_mux_map_enum,
				  MT6338_AUDENC_PMU_CON2,
				  RG_AUDPREAMPRINPUTSEL_SFT,
				  RG_AUDPREAMPRINPUTSEL_MASK,
				  pga_mux_map,
				  pga_mux_map_value);

static const struct snd_kcontrol_new pga_right_mux_control =
	SOC_DAPM_ENUM("PGA R Select", pga_right_mux_map_enum);

static const char * const pga_3_mux_map[] = {
	"AIN0", "AIN2", "AIN3", "AIN5"
};

static int pga_3_mux_map_value[] = {
	PGA_3_MUX_AIN0,
	PGA_3_MUX_AIN2,
	PGA_3_MUX_AIN3,
	PGA_3_MUX_AIN5
};

static SOC_VALUE_ENUM_SINGLE_DECL(pga_3_mux_map_enum,
				  MT6338_AUDENC_PMU_CON4,
				  RG_AUDPREAMP3INPUTSEL_SFT,
				  RG_AUDPREAMP3INPUTSEL_MASK,
				  pga_3_mux_map,
				  pga_3_mux_map_value);

static const struct snd_kcontrol_new pga_3_mux_control =
	SOC_DAPM_ENUM("PGA 3 Select", pga_3_mux_map_enum);


/* PGA 4 MUX */
static const char * const pga_4_mux_map[] = {
	"AIN2", "AIN3", "AIN4", "AIN6"
};

static int pga_4_mux_map_value[] = {
	PGA_4_MUX_AIN2,
	PGA_4_MUX_AIN3,
	PGA_4_MUX_AIN4,
	PGA_4_MUX_AIN6
};

static SOC_VALUE_ENUM_SINGLE_DECL(pga_4_mux_map_enum,
				  MT6338_AUDENC_PMU_CON6,
				  RG_AUDPREAMP4INPUTSEL_SFT,
				  RG_AUDPREAMP4INPUTSEL_MASK,
				  pga_4_mux_map,
				  pga_4_mux_map_value);

static const struct snd_kcontrol_new pga_4_mux_control =
	SOC_DAPM_ENUM("PGA 4 Select", pga_4_mux_map_enum);

static int mt_sgen_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol,
			 int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(priv->regmap, MT6338_AFE_SINEGEN_CON0,
			SINEGEN_DAC_EN_MASK_SFT,
			0x0 << SINEGEN_DAC_EN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_SINEGEN_CON1,
			SINEGEN_SINE_MODE_CH2_MASK_SFT,
			0xa << SINEGEN_SINE_MODE_CH2_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_SINEGEN_CON1,
			SINEGEN_SINE_MODE_CH1_MASK_SFT,
			0xa << SINEGEN_SINE_MODE_CH1_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_SINEGEN_CON2,
			SINEGEN_INNER_LOOP_BACK_MODE_MASK_SFT,
			0x13 << SINEGEN_INNER_LOOP_BACK_MODE_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_SINEGEN_CON3,
			SINEGEN_AMP_DIV_CH1_MASK_SFT,
			0x7 << SINEGEN_AMP_DIV_CH1_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_SINEGEN_CON3,
			SINEGEN_FREQ_DIV_CH1_MASK_SFT,
			0x1 << SINEGEN_FREQ_DIV_CH1_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_SINEGEN_CON4,
			SINEGEN_AMP_DIV_CH2_MASK_SFT,
			0x5 << SINEGEN_AMP_DIV_CH2_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_SINEGEN_CON4,
			SINEGEN_FREQ_DIV_CH2_MASK_SFT,
			0x2 << SINEGEN_FREQ_DIV_CH2_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		break;
	}

	return 0;
}

static void mtk_hp_enable(struct mt6338_priv *priv)
{
	dev_dbg(priv->dev, "%s()\n", __func__);

	if (priv->hp_hifi_mode != 0) {
		/* 0:normal path & bypass HWgain1/2 */
		regmap_write(priv->regmap, MT6338_AFE_TOP_DEBUG0, 0x4);
	} else {
		/* 3:hwgain1/2 swap & bypass HWgain1/2 */
		regmap_write(priv->regmap, MT6338_AFE_TOP_DEBUG0, 0xc4);

		/* Set to VD105 = 1V */
		if (priv->vd105 < 0xf)
			regmap_update_bits(priv->regmap, MT6338_STRUP_ELR_1,
				0x1f << 3, priv->vd105 << 3);
		else
			regmap_update_bits(priv->regmap, MT6338_STRUP_ELR_1,
				0x1f << 3, 0xf << 3);
	}

	if (priv->mux_select[MUX_HP_L] == HP_MUX_HPSPK) {
		/* Disable handset short-circuit protection */
		//todo
		/* Set LO DR bias current optimization, 010: 6uA */
		/* Set LO STB enhance circuits */
		/* Enable LO driver bias circuits */
		/* Enable LO driver core circuits */
		/* Set LO gain to 0DB */
	}
	ldo_select_to_min(priv, true, true);
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG1, 0xc8);
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG0, 0x1);
	nvreg_select_to_min(priv, true, true);

	/* Enable AUD_CLK */
	mt6338_set_decoder_clk(priv, true, priv->hp_hifi_mode);
	/* Enable HP mute */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPLMUTE_EN_VAUDP18_MASK_SFT |
		RG_HPRMUTE_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLMUTE_EN_VAUDP18_SFT);

	hp_pull_down(priv, false, true);

	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON3, 0xe3);

	/* Set HP NREG segmentation to min */
	regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON12, 0x0);
	if (priv->hp_hifi_mode == 0) {
		/* Set HP Trim code mode & Enable HPR/L LP test path mode */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON19, 0xaf);
		/* Switch HPL MUX to audio LODAC*/
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
			RG_AUDHPLMUXINPUTSEL_VAUDP18_MASK_SFT,
			HP_MUX_HS << RG_AUDHPLMUXINPUTSEL_VAUDP18_SFT);
		/* Switch HPL MUX to audio LODAC */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
			RG_AUDHPRMUXINPUTSEL_VAUDP18_MASK_SFT,
			HP_MUX_LOL << RG_AUDHPRMUXINPUTSEL_VAUDP18_SFT);
	} else {
		/* Set HP Trim code mode */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON19,	0x0c);
	}
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON11,
		RG_AUDHPTRIM_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPTRIM_EN_VAUDP18_SFT);
	/* Disable shortcut */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLSCDISABLE_VAUDP18_MASK_SFT |
		RG_AUDHPRSCDISABLE_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLSCDISABLE_VAUDP18_SFT);
	/* BAIS */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON31,
		RG_AUDIBIASPWRDN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDIBIASPWRDN_VAUDP18_SFT);

	if (priv->hp_hifi_mode) {
		/* Set HP DR bias current optimization, 010: 6uA */
		if (priv->hw_ver < 3)
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON30,
				RG_AUDBIASADJ_0_HP_VAUDP18_MASK_SFT,
				DRBIAS_6UA << RG_AUDBIASADJ_0_HP_VAUDP18_SFT);
		else
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON30,
				RG_AUDBIASADJ_0_HP_VAUDP18_MASK_SFT,
				DRBIAS_8UA << RG_AUDBIASADJ_0_HP_VAUDP18_SFT);
		/* Set HP & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON32, 0x55);
	} else {
		/* Set HP DR bias current optimization, 001: 5uA */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON30,
			RG_AUDBIASADJ_0_HP_VAUDP18_MASK_SFT,
			DRBIAS_6UA << RG_AUDBIASADJ_0_HP_VAUDP18_SFT);
		/* Set HP & ZCD bias current optimization */
		/* 00: ZCD: 3uA, HP/HS/LO: 4uA */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON32, 0x54);
	}

	/* NLE */
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_H,
		RG_DG_OUTPUT_DEBUG_MODE_LCH_MASK_SFT,
		0x1 << RG_DG_OUTPUT_DEBUG_MODE_LCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_H,
		RG_DG_OUTPUT_DEBUG_MODE_RCH_MASK_SFT,
		0x1 << RG_DG_OUTPUT_DEBUG_MODE_RCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H,
		RG_D2A_SIGNAL_SW_DEBUG_MODE_RCH_MASK_SFT,
		0x1 << RG_D2A_SIGNAL_SW_DEBUG_MODE_RCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L,
		RG_D2A_SIGNAL_SW_DEBUG_MODE_LCH_MASK_SFT,
		0x1 << RG_D2A_SIGNAL_SW_DEBUG_MODE_LCH_SFT);
	if (priv->hp_hifi_mode == 2) {
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L,
			RG_DA_ANA_HP_LNGAIN_ATT_LCH_MASK_SFT,
			0x1 << RG_DA_ANA_HP_LNGAIN_ATT_LCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H,
			RG_DA_ANA_HP_LNGAIN_ATT_RCH_MASK_SFT,
			0x1 << RG_DA_ANA_HP_LNGAIN_ATT_RCH_SFT);
	}
	/* Set HPP/N STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x7 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x7 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);

	/* HP Feedback Cap select 2'b00: 15pF */
	/* for >= 96KHz sampling rate: 2'b01: 10.5pF */
	if (priv->dl_rate[MT6338_AIF_1] >= 96000)
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
			RG_AUDHPHFCOMPBUFGAINSEL_VAUDP18_MASK_SFT,
			0x1 << RG_AUDHPHFCOMPBUFGAINSEL_VAUDP18_SFT);
	else
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
			RG_AUDHPHFCOMPBUFGAINSEL_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHPHFCOMPBUFGAINSEL_VAUDP18_SFT);

	/* Enable HP De-CMgain circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
		RG_AUDHPMAINCM_COMP_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPMAINCM_COMP_EN_VAUDP18_SFT);
	/* Disable 2nd order damp circuit when turn on sequence */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_DAMP2ND_EN_VAUDP18_MASK_SFT,
		0x0 << RG_DAMP2ND_EN_VAUDP18_SFT);
	/* Enable HD removed SW when turn on sequence */
	if (priv->hp_hifi_mode) {
		/* apply volume setting */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
			RG_HPLHDRM_PFL_EN_VAUDP18_MASK_SFT |
			RG_HPRHDRM_PFL_EN_VAUDP18_MASK_SFT,
			0x3 << RG_HPLHDRM_PFL_EN_VAUDP18_SFT);
	} else {
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
			RG_HPLHDRM_PFL_EN_VAUDP18_MASK_SFT |
			RG_HPRHDRM_PFL_EN_VAUDP18_MASK_SFT,
			0x0 << RG_HPLHDRM_PFL_EN_VAUDP18_SFT);
	}

	if (priv->hp_hifi_mode) {
		/* Set HP NREG segmentation to default value */
		regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON12, 0x70);
		/* Enable Neg R when turn on sequence */
		regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON12_H,
			DA_AUDHPLNEGR_EN_VAUDP18_MASK_SFT |
			DA_AUDHPRNEGR_EN_VAUDP18_MASK_SFT,
			0x3 << DA_AUDHPLNEGR_EN_VAUDP18_SFT);
	} else {
		regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON12_H,
			DA_AUDHPLNEGR_EN_VAUDP18_MASK_SFT |
			DA_AUDHPRNEGR_EN_VAUDP18_MASK_SFT,
			0x0 << DA_AUDHPLNEGR_EN_VAUDP18_SFT);
	}
	if (priv->hp_hifi_mode == 2) {
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H, 0x81);
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M, 0x17);
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L, 0x81);
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG, 0x17);
		regmap_write(priv->regmap, MT6338_AFE_NLE_CFG, 0x1);
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H, 0x81);
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M, 0x40);
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L, 0x81);
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG, 0x40);
	} else {
		/*Enable HPR/L main output stage to min*/
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
			RG_DA_HP_OUTSTG_RCH_MASK_SFT,
			0x0 << RG_DA_HP_OUTSTG_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
			RG_DA_HP_OUTSTG_LCH_MASK_SFT,
			0x0 << RG_DA_HP_OUTSTG_LCH_SFT);
	}
	/* Damping adjustment select. (Hi-Fi mode) */
	if (priv->hp_hifi_mode)
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
			RG_AUDHPDAMP_ADJ_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHPDAMP_ADJ_VAUDP18_SFT);
	else
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
			RG_AUDHPDAMP_ADJ_VAUDP18_MASK_SFT,
			0x2 << RG_AUDHPDAMP_ADJ_VAUDP18_SFT);
	/* Enable HP damping ckt.  */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_AUDHPDAMP_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPDAMP_EN_VAUDP18_SFT);

	/* Enable HFOP circuits for Hi-Fi mode */
	if (priv->hp_hifi_mode == 1) {
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
			RG_AUDHPHFOP_EN_VAUDP18_MASK_SFT,
			0x1 << RG_AUDHPHFOP_EN_VAUDP18_SFT);
	} else if (priv->hp_hifi_mode == 2) {
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
			RG_AUDHPHFOP_EN_VAUDP18_MASK_SFT |
			RG_AUDHPMAINCM_COMP_EN_VAUDP18_MASK_SFT,
			0x3 << RG_AUDHPHFOP_EN_VAUDP18_SFT);
	} else {
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
			RG_AUDHPHFOP_EN_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHPHFOP_EN_VAUDP18_SFT);
	}

	/* Set input diff pair bias to min (20uA)*/
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
		RG_AUDHPDIFFINPBIASADJ_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPDIFFINPBIASADJ_VAUDP18_SFT);

	/* Enable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTAUXPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTAUXPWRUP_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLOUTAUXPWRUP_VAUDP18_SFT);
	/* Enable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLAUXFBRSW_EN_VAUDP18_MASK_SFT |
		RG_HPRAUXFBRSW_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLAUXFBRSW_EN_VAUDP18_SFT);
	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_AUDHPLAUXCM_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRAUXCM_EN_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLAUXCM_EN_VAUDP18_SFT);
	/* Enable HP driver bias circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPLPWRUP_IBIAS_VAUDP18_MASK_SFT |
		RG_AUDHPRPWRUP_IBIAS_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLPWRUP_IBIAS_VAUDP18_SFT);
	/* Enable HP driver core circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPLPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPRPWRUP_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLPWRUP_VAUDP18_SFT);
	/* Short HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLSHORT2HPLAUX_EN_VAUDP18_MASK_SFT |
		RG_HPRSHORT2HPRAUX_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLSHORT2HPLAUX_EN_VAUDP18_SFT);
	if (priv->hp_hifi_mode) {
		/* Increase HP input pair current to HPM step by step */
		hp_in_pair_current(priv, true);
	}
	if (priv->hp_hifi_mode == 2) {
		/* Enable HP main CMFB loop */
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
			RG_DA_HP_LNSW_EN_RCH_MASK_SFT |
			RG_DA_HPCMFB_LN_EN_RCH_MASK_SFT,
			0x3 << RG_DA_HPCMFB_LN_EN_RCH_SFT);
		usleep_range(600, 620);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
			RG_DA_HP_LNSW_EN_LCH_MASK_SFT |
			RG_DA_HPCMFB_LN_EN_LCH_MASK_SFT,
			0x3 << RG_DA_HPCMFB_LN_EN_LCH_SFT);
		usleep_range(600, 620);
	} else {
		/* Enable HP main CMFB loop */
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
			RG_DA_HPCMFB_EN_RCH_MASK_SFT,
			0x1 << RG_DA_HPCMFB_EN_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
			RG_DA_HPCMFB_EN_LCH_MASK_SFT,
			0x1 << RG_DA_HPCMFB_EN_LCH_SFT);
	}
	/* Disable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_AUDHPLAUXCM_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRAUXCM_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLAUXCM_EN_VAUDP18_SFT);

	/* Enable HP main output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTPWRUP_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLOUTPWRUP_VAUDP18_SFT);

	/* Enable HPR/L main output stage step by step */
	hp_main_output_ramp(priv, true);

	/* Reduce HP aux feedback loop gain */
	hp_aux_feedback_loop_gain_ramp(priv, true);
	/* Disable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLAUXFBRSW_EN_VAUDP18_MASK_SFT |
		RG_HPRAUXFBRSW_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLAUXFBRSW_EN_VAUDP18_SFT);

	/* apply volume setting */
	headset_volume_ramp(priv, -1, HP_GAIN_0DB);

	/* Disable HP mute */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPLMUTE_EN_VAUDP18_MASK_SFT |
		RG_HPRMUTE_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLMUTE_EN_VAUDP18_SFT);
	/* apply volume setting */
	headset_volume_ramp(priv,
			    HP_GAIN_0DB,
			    priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL]);

	/* open HP output to AUX output */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLSHORT2HPLAUX_EN_VAUDP18_MASK_SFT |
		RG_HPRSHORT2HPRAUX_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLSHORT2HPLAUX_EN_VAUDP18_SFT);
	/* Disable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTAUXPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTAUXPWRUP_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLOUTAUXPWRUP_VAUDP18_SFT);
	/* Reset HPP/N STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);
	/* Disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true, false);
	/* Enable HFOP circuits for Hi-Fi mode */
	if (priv->hp_hifi_mode)
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
			RG_DAMP2ND_EN_VAUDP18_MASK_SFT,
			0x1 << RG_DAMP2ND_EN_VAUDP18_SFT);

	/* CMFB resistor with modulation Rwell levele */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLCMFB_RNWSEL_VAUDP18_MASK_SFT |
		RG_AUDHPRCMFB_RNWSEL_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLCMFB_RNWSEL_VAUDP18_SFT);
	/* Feedback resistor with modulation Rwell level */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLHPFB_RNWSEL_VAUDP18_MASK_SFT |
		RG_AUDHPRHPFB_RNWSEL_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLHPFB_RNWSEL_VAUDP18_SFT);

	/* Enable HP feedback SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
		RG_AUDHPHIFISWST_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPHIFISWST_EN_VAUDP18_SFT);

	if (priv->hp_hifi_mode) {
		/* Enable HD removed SW source-tie for Hi-Fi mode */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON10,
			RG_HPLHDRMSW_ST_EN_VAUDP18_MASK_SFT |
			RG_HPRHDRMSW_ST_EN_VAUDP18_MASK_SFT,
			0x3 << RG_HPLHDRMSW_ST_EN_VAUDP18_SFT);
	}

	/* Enable CMFB SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON12_H,
		DA_HPLCMFBSWST_EN_VAUDP18_MASK_SFT |
		DA_HPRCMFBSWST_EN_VAUDP18_MASK_SFT,
		0x3 << DA_HPLCMFBSWST_EN_VAUDP18_SFT);
	/* Enable HP input MUX SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_HPMUXST_EN_VAUDP18_MASK_SFT,
		0x1 << RG_HPMUXST_EN_VAUDP18_SFT);
	/* [7:6] HP 2nd order damp control
	 *  damp adjustimation automatically select by NLE
	 */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON49,
		0x3 << (RG_ABIDEC_RSVD1_VAUDP18_SFT + 6),
		0x3 << (RG_ABIDEC_RSVD1_VAUDP18_SFT + 6));
	/* Enable HPRL LN path feedback Rwell modulation */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLMAINCM2_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRMAINCM2_EN_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLMAINCM2_EN_VAUDP18_SFT);

	if (priv->hp_hifi_mode) {
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON0,
			RG_AUDDACHPL_TRIM_EN_VAUDP18_MASK_SFT,
			0x1 << RG_AUDDACHPL_TRIM_EN_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON2,
			RG_AUDDACHPR_TRIM_EN_VAUDP18_MASK_SFT,
			0x1 << RG_AUDDACHPR_TRIM_EN_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON8,
			RG_AUDDACHP_HOLD_SW_EN_VAUDP18_MASK_SFT,
			0x0 << RG_AUDDACHP_HOLD_SW_EN_VAUDP18_SFT);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON3, 0xff);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON3, 0x9f);
	} else {
		regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON9, 0x0);
		regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON10, 0x0);
	}
	/* Set DAC as 512uA mode */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x0);
	if (priv->hp_hifi_mode) {
		/* Enable Audio DAC  */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACL_PWRUP_VAUDP18_MASK_SFT |
			RG_AUDDACR_PWRUP_VAUDP18_MASK_SFT,
			0x3 << RG_AUDDACL_PWRUP_VAUDP18_SFT);
		usleep_range(600, 620);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACL_BIAS_PWRUP_VA32_MASK_SFT |
			RG_AUDDACR_BIAS_PWRUP_VA32_MASK_SFT,
			0x3 << RG_AUDDACL_BIAS_PWRUP_VA32_SFT);
	} else {
		/* Enable Audio HS&LO DAC */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACHS_PWRUP_VAUDP18_MASK_SFT |
			RG_AUDDACLO_PWRUP_VAUDP18_MASK_SFT,
			0x3 << RG_AUDDACHS_PWRUP_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACHS_BIAS_PWRUP_VA32_MASK_SFT |
			RG_AUDDACLO_BIAS_PWRUP_VA32_MASK_SFT,
			0x3 << RG_AUDDACHS_BIAS_PWRUP_VA32_SFT);
	}

	/* Select to AVDD30_AUD  */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON43, 0x1);
	usleep_range(100, 120);

	if (priv->hp_hifi_mode) {
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON1, 0x70);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON2, 0x17);
	}

	if (priv->mux_select[MUX_HP_L] == HP_MUX_HPSPK) {
		/* Switch HPL MUX to audio LOL */
		//todo
		/* Switch LOL MUX to audio DACL */
	}

	if (priv->hp_hifi_mode) {
		/* Switch HPL MUX to audio DAC */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
			RG_AUDHPLMUXINPUTSEL_VAUDP18_MASK_SFT,
			HP_MUX_HP << RG_AUDHPLMUXINPUTSEL_VAUDP18_SFT);
		/* Switch HPR MUX to audio DAC */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
			RG_AUDHPRMUXINPUTSEL_VAUDP18_MASK_SFT,
			HP_MUX_HP << RG_AUDHPRMUXINPUTSEL_VAUDP18_SFT);
	}
	if (priv->hp_hifi_mode == 2) {
		/* NLE mode and release debug mode */
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
			RG_DA_HP_OUTSTG_LN_RCH_MASK_SFT,
			0x1 << RG_DA_HP_OUTSTG_LN_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
			RG_DA_HP_OUTSTG_LN_LCH_MASK_SFT,
			0x1 << RG_DA_HP_OUTSTG_LN_LCH_SFT);
		usleep_range(600, 620);
		/* Disable HPR/L main output stage step by step */
		hp_main_output_ramp(priv, false);
		usleep_range(600, 620);
		hp_ln_gain_ramp(priv, true);

		/* Need larger than preview window	delay 0.0001 */
		usleep_range(100, 120);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_H,
			RG_DG_OUTPUT_DEBUG_MODE_RCH_MASK_SFT,
			0x0 << RG_DG_OUTPUT_DEBUG_MODE_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_H,
			RG_DG_OUTPUT_DEBUG_MODE_LCH_MASK_SFT,
			0x0 << RG_DG_OUTPUT_DEBUG_MODE_LCH_SFT);
		usleep_range(600, 620);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H,
			RG_D2A_SIGNAL_SW_DEBUG_MODE_RCH_MASK_SFT,
			0x0 << RG_D2A_SIGNAL_SW_DEBUG_MODE_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L,
			RG_D2A_SIGNAL_SW_DEBUG_MODE_LCH_MASK_SFT,
			0x0 << RG_D2A_SIGNAL_SW_DEBUG_MODE_LCH_SFT);
		usleep_range(600, 620);
	}
}

static void mtk_hp_disable(struct mt6338_priv *priv)
{
	if (priv->hp_hifi_mode == 0) {
		/* Set to VD105 = 1.5V */
		regmap_update_bits(priv->regmap, MT6338_STRUP_ELR_1,
			0x1f << 3, (priv->vd105 - 0xc) << 3);
	}

	if (priv->hp_hifi_mode == 2) {
		/* Set NLE DA signal to debug mode */
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H,
			RG_D2A_SIGNAL_SW_DEBUG_MODE_RCH_MASK_SFT,
			0x1 << RG_D2A_SIGNAL_SW_DEBUG_MODE_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L,
			RG_D2A_SIGNAL_SW_DEBUG_MODE_LCH_MASK_SFT,
			0x1 << RG_D2A_SIGNAL_SW_DEBUG_MODE_LCH_SFT);
		/* digital gain output debug mode */
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_H,
			RG_DG_OUTPUT_DEBUG_MODE_LCH_MASK_SFT,
			0x1 << RG_DG_OUTPUT_DEBUG_MODE_LCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_H,
			RG_DG_OUTPUT_DEBUG_MODE_RCH_MASK_SFT,
			0x1 << RG_DG_OUTPUT_DEBUG_MODE_RCH_SFT);
		usleep_range(100, 120);

		hp_ln_gain_ramp(priv, false);
		/* Enable HPR/L main output stage step by step */
		hp_main_output_ramp(priv, true);

		usleep_range(100, 120);
		/* NLE mode and release debug mode */
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
			RG_DA_HP_OUTSTG_LN_LCH_MASK_SFT,
			0x0 << RG_DA_HP_OUTSTG_LN_LCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
			RG_DA_HP_OUTSTG_LN_RCH_MASK_SFT,
			0x0 << RG_DA_HP_OUTSTG_LN_RCH_SFT);

	}
	/* Disable LO when MUX to HPSPK */
	if (priv->mux_select[MUX_HP_L] == HP_MUX_HPSPK) {
		/* Switch LOL MUX to open */
		//todo
		/* decrease LO gain to minimum gain step by step */
		/* Disable LO driver core circuits */
		/* Disable LO driver bias circuits */
	}

	if (priv->hp_hifi_mode) {
		/* Disable low-noise mode of DAC */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON2,
			RG_AUDDAC_OPAMP_LN_EN_VA32_MASK_SFT,
			0x0 << RG_AUDDAC_OPAMP_LN_EN_VA32_SFT);

		/* Scrambler/RPMW selection */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON1,
			RG_AUDDAC_R_RPWM_EN_VAUDP18_MASK_SFT |
			RG_AUDDAC_L_RPWM_EN_VAUDP18_MASK_SFT,
			0x0 << RG_AUDDAC_L_RPWM_EN_VAUDP18_SFT);
	}
	/* Select to AVDD30_AUD  */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON43, 0x0);

	/* Disable Audio DAC */
	if (priv->hp_hifi_mode) {
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACL_PWRUP_VAUDP18_MASK_SFT |
			RG_AUDDACR_PWRUP_VAUDP18_MASK_SFT,
			0x0 << RG_AUDDACL_PWRUP_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACL_BIAS_PWRUP_VA32_MASK_SFT |
			RG_AUDDACR_BIAS_PWRUP_VA32_MASK_SFT,
			0x0 << RG_AUDDACL_BIAS_PWRUP_VA32_SFT);
	} else {
		/* Disable Audio HS&LO DAC */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACHS_PWRUP_VAUDP18_MASK_SFT |
			RG_AUDDACLO_PWRUP_VAUDP18_MASK_SFT,
			0x0 << RG_AUDDACHS_PWRUP_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACHS_BIAS_PWRUP_VA32_MASK_SFT |
			RG_AUDDACLO_BIAS_PWRUP_VA32_MASK_SFT,
			0x0 << RG_AUDDACHS_BIAS_PWRUP_VA32_SFT);
	}
	/* CMFB resistor with modulation Rwell levele */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLCMFB_RNWSEL_VAUDP18_MASK_SFT |
		RG_AUDHPRCMFB_RNWSEL_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLCMFB_RNWSEL_VAUDP18_SFT);
	/* Feedback resistor with modulation Rwell level */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLHPFB_RNWSEL_VAUDP18_MASK_SFT |
		RG_AUDHPRHPFB_RNWSEL_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLHPFB_RNWSEL_VAUDP18_SFT);
	/* Enable HP feedback SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
		RG_AUDHPHIFISWST_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPRHPFB_RNWSEL_VAUDP18_SFT);
	if (priv->hp_hifi_mode) {
		/* Enable HD removed SW source-tie for Hi-Fi mode */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON10,
			RG_HPLHDRMSW_ST_EN_VAUDP18_MASK_SFT,
			0x0 << RG_HPLHDRMSW_ST_EN_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON10,
			RG_HPRHDRMSW_ST_EN_VAUDP18_MASK_SFT,
			0x0 << RG_HPRHDRMSW_ST_EN_VAUDP18_SFT);
	}
	/* Enable CMFB SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON12_H,
		DA_HPLCMFBSWST_EN_VAUDP18_MASK_SFT |
		DA_HPRCMFBSWST_EN_VAUDP18_MASK_SFT,
		0x0 << DA_HPLCMFBSWST_EN_VAUDP18_SFT);
	/* Enable HP input MUX SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_HPMUXST_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPMUXST_EN_VAUDP18_SFT);

	/* Enable 2nd order damp circuit for Hi-Fi mode*/
	if (priv->hp_hifi_mode)
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
			RG_DAMP2ND_EN_VAUDP18_MASK_SFT,
			0x0 << RG_DAMP2ND_EN_VAUDP18_SFT);

	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true, true);

	/* Reset HPP/N STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x7 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x7 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);
	/* Short HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTAUXPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTAUXPWRUP_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLOUTAUXPWRUP_VAUDP18_SFT);
	/* Enable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLSHORT2HPLAUX_EN_VAUDP18_MASK_SFT |
		RG_HPRSHORT2HPRAUX_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLSHORT2HPLAUX_EN_VAUDP18_SFT);

	/* decrease HPL/R gain to normal gain step by step */
	headset_volume_ramp(priv,
				priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL],
				HP_GAIN_0DB);

	/* Disable HP mute */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPLMUTE_EN_VAUDP18_MASK_SFT |
		RG_HPRMUTE_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLMUTE_EN_VAUDP18_SFT);

	/* Disable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLAUXFBRSW_EN_VAUDP18_MASK_SFT |
		RG_HPRAUXFBRSW_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLAUXFBRSW_EN_VAUDP18_SFT);
	/* Reduce HP aux feedback loop gain */
	hp_aux_feedback_loop_gain_ramp(priv, false);

	/* decrease HPR/L main output stage step by step */
	hp_main_output_ramp(priv, false);

	/* Disable HP main output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTPWRUP_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLOUTPWRUP_VAUDP18_SFT);

	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_AUDHPLAUXCM_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRAUXCM_EN_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLAUXCM_EN_VAUDP18_SFT);

	if (priv->hp_hifi_mode == 2) {
		/* Disable HP main CMFB loop */
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
			RG_DA_HPCMFB_LN_EN_RCH_MASK_SFT,
			0x0 << RG_DA_HPCMFB_LN_EN_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
			RG_DA_HPCMFB_LN_EN_LCH_MASK_SFT,
			0x0 << RG_DA_HPCMFB_LN_EN_LCH_SFT);
	} else {
		/* Disable HP main CMFB loop */
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
			RG_DA_HPCMFB_EN_RCH_MASK_SFT,
			0x0 << RG_DA_HPCMFB_EN_RCH_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
			RG_DA_HPCMFB_EN_LCH_MASK_SFT,
			0x0 << RG_DA_HPCMFB_EN_LCH_SFT);
	}
	if (priv->hp_hifi_mode) {
		/* Decrease HP input pair current to 2'b00 step by step */
		hp_in_pair_current(priv, false);
	}
	/* open HP output to AUX output */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLSHORT2HPLAUX_EN_VAUDP18_MASK_SFT |
		RG_HPRSHORT2HPRAUX_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLSHORT2HPLAUX_EN_VAUDP18_SFT);
	/* Disable HP driver core circuits */
	/* Disable HP driver bias circuits */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON4, 0x0);

	/* Disable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTAUXPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTAUXPWRUP_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLOUTAUXPWRUP_VAUDP18_SFT);

	/* Disable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLAUXFBRSW_EN_VAUDP18_MASK_SFT |
		RG_HPRAUXFBRSW_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLAUXFBRSW_EN_VAUDP18_SFT);

	/* Disable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_AUDHPLAUXCM_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRAUXCM_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLAUXCM_EN_VAUDP18_SFT);
	if (priv->hp_hifi_mode) {
		/* Disable HD removed SW when turn on sequence */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
			RG_HPLHDRM_PFL_EN_VAUDP18_MASK_SFT |
			RG_HPRHDRM_PFL_EN_VAUDP18_MASK_SFT,
			0x0 << RG_HPLHDRM_PFL_EN_VAUDP18_SFT);
		/* Disable Neg R when turn on sequence */
		regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON12_H,
			DA_AUDHPLNEGR_EN_VAUDP18_MASK_SFT |
			DA_AUDHPRNEGR_EN_VAUDP18_MASK_SFT,
			0x0 << DA_AUDHPLNEGR_EN_VAUDP18_SFT);
		/* Set HP NREG segmentation */
		regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON12, 0x0);
		/* Enable HFOP circuits for Hi-Fi mode */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
			RG_AUDHPHFOP_EN_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHPHFOP_EN_VAUDP18_SFT);
	}
	/* Enable HP damping ckt. */
	/* Enable HP damping ckt. */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON15, 0x0);
	/* Damping adjustment select. (Hi-Fi mode) */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON14, 0x0);
	/*Enable HPR/L main output stage to min*/
	regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M, 0x0);
	regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG, 0x0);
	/* Enable HP De-CMgain circuits */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON16, 0x0);

	/* Set HPP/N STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);


	/* Disable NLE */
	regmap_write(priv->regmap, MT6338_AFE_NLE_CFG, 0x0);

	if (priv->hp_hifi_mode == 0) {
		/* Set HP Trim code mode */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON19, 0x0c);
	}
	if (priv->hp_hifi_mode == 2) {
		/* NLE */
		/* Set NLE DA signal to debug mode */
		/* Set NLE DA signal to debug mode */
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_H, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_H, 0x0);
	}
	/* BAIS */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON31,
		RG_AUDIBIASPWRDN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDIBIASPWRDN_VAUDP18_SFT);
	/* Disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true, false);

	/* Set HP damp parameter MSB & enable HP FB */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON3, 0xe3);
	/* Enable for NVREG */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON41,
		RG_NVREG_DACSW_L_EN_VAUDP18_MASK_SFT |
		RG_NVREG_DACSW_R_EN_VAUDP18_MASK_SFT,
		0x0 << RG_NVREG_DACSW_L_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON41,
		RG_NVREG_LC_BUF_L_EN_VAUDP18_MASK_SFT |
		RG_NVREG_LC_BUF_R_EN_VAUDP18_MASK_SFT,
		0x0 << RG_NVREG_LC_BUF_L_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON41,
		RG_NVREG_HC_BUF_L_EN_VAUDP18_MASK_SFT |
		RG_NVREG_HC_BUF_R_EN_VAUDP18_MASK_SFT,
		0x0 << RG_NVREG_HC_BUF_L_EN_VAUDP18_SFT);
	/* Disable NCP */
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG0, 0x0);
	ldo_select_to_min(priv, false, true);

	/* Disable AUD_CLK */
	mt6338_set_decoder_clk(priv, false, true);

	/* 0:normal path */
	regmap_update_bits(priv->regmap, MT6338_AFE_TOP_DEBUG0,
		0x3 << 0x6, 0x0 << 0x6);
}

static int mtk_hp_impedance_enable(struct mt6338_priv *priv)
{
	/* 0:normal path & bypass HWgain1/2 */
	regmap_write(priv->regmap, MT6338_AFE_TOP_DEBUG0, 0x4);

	/* Enable AUD_CLK */
	mt6338_set_decoder_clk(priv, true, true);

	ldo_select_to_min(priv, true, true);
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG1, 0xCB);
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG0, 0x1);
	nvreg_select_to_min(priv, true, true);

	/* Disable HP mute */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPLMUTE_EN_VAUDP18_MASK_SFT |
		RG_HPRMUTE_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLMUTE_EN_VAUDP18_SFT);

	/* Disable shortcut */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLSCDISABLE_VAUDP18_MASK_SFT |
		RG_AUDHPRSCDISABLE_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLSCDISABLE_VAUDP18_SFT);

	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON3, 0x1c);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x0 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x0 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);

	/* Disable Pull-down HPL/R to AVSS28_AUD */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON11,
		RG_HPPSHORT2VCM_VAUDP18_MASK_SFT,
		0x0 << RG_HPPSHORT2VCM_VAUDP18_SFT);
	/* Disable HP damping circuit & HPN 4K load */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON48, 0x0);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON0,
		RG_AUDDACHPL_TRIM_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDDACHPL_TRIM_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON2,
		RG_AUDDACHPR_TRIM_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDDACHPR_TRIM_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON8,
		RG_AUDDACHP_HOLD_SW_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDDACHP_HOLD_SW_EN_VAUDP18_SFT);

	/* Enable TRIMBUF circuit, select HPR as TRIMBUF input */
	/* Enable TRIMBUF circuit 2nd stage */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x0);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
		RG_AUDDACL_PWRUP_VAUDP18_MASK_SFT,
		0x1 << RG_AUDDACL_PWRUP_VAUDP18_SFT);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
		RG_AUDDACL_BIAS_PWRUP_VA32_MASK_SFT,
		0x1 << RG_AUDDACL_BIAS_PWRUP_VA32_SFT);

	/* Select to AVDD30_AUD */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON43, 0x1);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON1,
		RG_AUDDAC_L_RPWM_EN_VAUDP18_MASK_SFT |
		RG_AUDDAC_R_RPWM_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDDAC_L_RPWM_EN_VAUDP18_SFT);

	/* Enable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON1,
		RG_AUDDAC_IREF_LN_SEL_VA32_MASK_SFT,
		0x3 << RG_AUDDAC_IREF_LN_SEL_VA32_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON1,
		RG_AUDDAC_IREF_LN_EN_VA32_MASK_SFT,
		0x1 << RG_AUDDAC_IREF_LN_EN_VA32_SFT);
	/* Enable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON2,
		RG_AUDDAC_ILOCAL_LN_SEL_VA32_MASK_SFT,
		0x3 << RG_AUDDAC_ILOCAL_LN_SEL_VA32_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON2,
		RG_AUDDAC_ILOCAL_LN_EN_VA32_MASK_SFT,
		0x1 << RG_AUDDAC_ILOCAL_LN_EN_VA32_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON2,
		RG_AUDDAC_OPAMP_LN_EN_VA32_MASK_SFT,
		0x1 << RG_AUDDAC_OPAMP_LN_EN_VA32_SFT);

	/* Enable HPDET circuit */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON29,
		RG_AUDHPSPKDET_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPSPKDET_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON29,
		RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP18_SFT);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON29,
		RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP18_MASK_SFT,
		0x2 << RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP18_SFT);
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47,	0x1);
	return 0;
}

static int mtk_hp_impedance_disable(struct mt6338_priv *priv)
{
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x0);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON0,
		RG_AUDDACHPL_TRIM_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDDACHPL_TRIM_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON2,
		RG_AUDDACHPR_TRIM_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDDACHPR_TRIM_EN_VAUDP18_SFT);
	/* Disable HPDET circuit, select OPEN as HPDET input */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON29,
		RG_AUDHPSPKDET_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPSPKDET_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON29,
		RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON29,
		RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP18_SFT);
	/* Disable low-noise mode of DAC */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON1, 0x0);
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON2, 0x0);
	/* Disable Audio L channel DAC */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
		RG_AUDDACL_PWRUP_VAUDP18_MASK_SFT,
		0x0 << RG_AUDDACL_PWRUP_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
		RG_AUDDACL_BIAS_PWRUP_VA32_MASK_SFT,
		0x0 << RG_AUDDACL_BIAS_PWRUP_VA32_SFT);
	/* Disable NCP */
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG0, 0x0);
	/* Disable AUD_CLK */
	mt6338_set_decoder_clk(priv, false, true);

	/* Enable HPR/L STB enhance circuits for off state */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);

#if IS_ENABLED(CONFIG_SND_SOC_MT6338_ACCDET)
	/* from accdet request */
	mt6338_accdet_modify_vref_volt();
#endif
	return 0;
}

static int mt_hp_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol,
		       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);
	int device = DEVICE_HP;

	dev_info(priv->dev, "%s(), event 0x%x, dev_counter[DEV_HP] %d, mux %u\n",
		 __func__, event, priv->dev_counter[device], mux);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		priv->dev_counter[device]++;
		if (priv->dev_counter[device] > 1)
			break;	/* already enabled, do nothing */
		else if (priv->dev_counter[device] <= 0)
			dev_warn(priv->dev, "%s(), dev_counter[DEV_HP] %d <= 0\n",
				 __func__,
				 priv->dev_counter[device]);

		priv->mux_select[MUX_HP_L] = mux;

		if (mux == HP_MUX_HP || mux == HP_MUX_HPSPK)
			mtk_hp_enable(priv);
		else if (mux == HP_MUX_HP_IMPEDANCE)
			mtk_hp_impedance_enable(priv);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		priv->dev_counter[device]--;
		if (priv->dev_counter[device] > 0)
			break;	/* still being used, don't close */
		else if (priv->dev_counter[device] < 0) {
			dev_warn(priv->dev, "%s(), dev_counter[DEV_HP] %d < 0\n",
				 __func__,
				 priv->dev_counter[device]);
			priv->dev_counter[device] = 0;
			break;
		}

		if (priv->mux_select[MUX_HP_L] == HP_MUX_HP ||
		    priv->mux_select[MUX_HP_L] == HP_MUX_HPSPK)
			mtk_hp_disable(priv);
		else if (priv->mux_select[MUX_HP_L] == HP_MUX_HP_IMPEDANCE)
			mtk_hp_impedance_disable(priv);

		priv->mux_select[MUX_HP_L] = mux;
		break;
	default:
		break;
	}

	return 0;
}

static int mt_rcv_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event 0x%x, mux %u\n",
		 __func__, event, dapm_kcontrol_get_value(w->kcontrols[0]));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* 3:hwgain1/2 swap & bypass HWgain1/2 */
		regmap_write(priv->regmap, MT6338_AFE_TOP_DEBUG0, 0xc4);

		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x27);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON36, 0x22);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON37, 0x15);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON39, 0x15);

		regmap_write(priv->regmap, MT6338_AFE_NCP_CFG1, 0xcb);
		regmap_write(priv->regmap, MT6338_AFE_NCP_CFG0, 0x1);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON33, 0x33);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON34, 0x3);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON41, 0x15);
		/* Disable handset short-circuit protection */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON20,
			RG_AUDHSSCDISABLE_VAUDP18_MASK_SFT,
			0x1 << RG_AUDHSSCDISABLE_VAUDP18_SFT);
		/* IBIST */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON31,
			RG_AUDIBIASPWRDN_VAUDP18_MASK_SFT,
			0x0 << RG_AUDIBIASPWRDN_VAUDP18_SFT);
		/* Set RCV DR bias current optimization, 010: 6uA */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON30,
			RG_AUDBIASADJ_0_HS_VAUDP18_MASK_SFT,
			DRBIAS_6UA << RG_AUDBIASADJ_0_HS_VAUDP18_SFT);
		/* Set RCV & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		if (priv->dev_counter[DEVICE_HP] == 0)
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON32,
					   IBIAS_ZCD_MASK_SFT,
					   IBIAS_ZCD_4UA << IBIAS_ZCD_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON32,
			IBIAS_HS_MASK_SFT,
			IBIAS_5UA << IBIAS_HS_SFT);

		/* Set HS STB enhance circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON23,
			RG_HSOUTPUTSTBENH_VAUDP18_MASK_SFT,
			0x1 << RG_HSOUTPUTSTBENH_VAUDP18_SFT);
		/* Set HS output stage (3'b111 = 8x) */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON23,
			RG_HSOUTSTGCTRL_VAUDP18_MASK_SFT,
			0x7 << RG_HSOUTSTGCTRL_VAUDP18_SFT);
		/* Enable HS driver bias circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON20,
			RG_AUDHSPWRUP_IBIAS_VAUDP18_MASK_SFT,
			0x1 << RG_AUDHSPWRUP_IBIAS_VAUDP18_SFT);
		/* Enable HS driver core circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON20,
			RG_AUDHSPWRUP_VAUDP18_MASK_SFT,
			0x1 << RG_AUDHSPWRUP_VAUDP18_SFT);

		/* Set HS gain to normal gain step by step */
		regmap_write(priv->regmap, MT6338_ZCD_CON3,
			priv->ana_gain[AUDIO_ANALOG_VOLUME_HSOUTL]);

		/* Enable AUD_CLK */
		mt6338_set_decoder_clk(priv, true, false);

		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACHS_PWRUP_VAUDP18_MASK_SFT,
			0x1 << RG_AUDDACHS_PWRUP_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACHS_BIAS_PWRUP_VA32_MASK_SFT,
			0x1 << RG_AUDDACHS_BIAS_PWRUP_VA32_SFT);

		/* ldo_select_to_min(priv, false, false); */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON36, 0x2);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x7);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON36, 0x0);

		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON33, 0x0);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON34, 0x0);

		/* Enable Audio DAC  */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON43, 0x1);

		/* Enable low-noise mode of DAC */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x2);
		/* Switch HS MUX to audio DAC */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON22,
			RG_AUDHSMUXINPUTSEL_VAUDP18_MASK_SFT,
			RCV_MUX_VOICE_PLAYBACK << RG_AUDHSMUXINPUTSEL_VAUDP18_SFT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* Disable low-noise mode of DAC */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x0);

		/* HS mux to open */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON22,
			RG_AUDHSMUXINPUTSEL_VAUDP18_MASK_SFT,
			RCV_MUX_OPEN << RG_AUDHSMUXINPUTSEL_VAUDP18_SFT);

		/* Disable Audio DAC */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACHS_PWRUP_VAUDP18_MASK_SFT,
			0x0 << RG_AUDDACHS_PWRUP_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACHS_BIAS_PWRUP_VA32_MASK_SFT,
			0x0 << RG_AUDDACHS_BIAS_PWRUP_VA32_SFT);

		/* Disable AUD_CLK */
		mt6338_set_decoder_clk(priv, false, false);

		/* decrease HS gain to minimum gain step by step */
		regmap_write(priv->regmap, MT6338_ZCD_CON3,
			DL_GAIN_N_40DB);
		/* Disable HS driver core circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON20,
			RG_AUDHSPWRUP_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHSPWRUP_VAUDP18_SFT);
		/* Disable HS driver bias circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON20,
			RG_AUDHSPWRUP_IBIAS_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHSPWRUP_IBIAS_VAUDP18_SFT);
		/* 0:normal path */
		regmap_update_bits(priv->regmap, MT6338_AFE_TOP_DEBUG0,
			0x3 << 0x6, 0x0 << 0x6);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_lo_event(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol,
		       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);
	dev_info(priv->dev, "%s(), event 0x%x, mux %u\n", __func__, event, mux);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* 3:hwgain1/2 swap & bypass HWgain1/2, 0:normal path */
		regmap_write(priv->regmap, MT6338_AFE_TOP_DEBUG0, 0xc4);

		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x27);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON36, 0x22);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON37, 0x3f);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON39, 0x3f);

		regmap_write(priv->regmap, MT6338_AFE_NCP_CFG1, 0xcb);
		regmap_write(priv->regmap, MT6338_AFE_NCP_CFG0, 0x1);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON33, 0x33);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON34, 0x3);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON41, 0x3f);


		/* Disable handset short-circuit protection */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON24,
			RG_AUDLOSCDISABLE_VAUDP18_MASK_SFT,
			0x1 << RG_AUDLOSCDISABLE_VAUDP18_SFT);
		/* IBIST */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON31,
			RG_AUDIBIASPWRDN_VAUDP18_MASK_SFT,
			0x0 << RG_AUDIBIASPWRDN_VAUDP18_SFT);

		/* Set LO DR bias current optimization, 010: 6uA */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON31,
			RG_AUDBIASADJ_0_LO_VAUDP18_MASK_SFT,
			DRBIAS_6UA << RG_AUDBIASADJ_0_LO_VAUDP18_SFT);

		/* Set LO & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		if (priv->dev_counter[DEVICE_HP] == 0)
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON32,
				IBIAS_ZCD_MASK_SFT,
				IBIAS_ZCD_4UA << IBIAS_ZCD_SFT);

		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON32,
			IBIAS_LO_MASK_SFT,
			IBIAS_6UA << IBIAS_LO_SFT);

		/* Set LO STB enhance circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON27,
			RG_LOOUTPUTSTBENH_VAUDP18_MASK_SFT,
			0x1 << RG_LOOUTPUTSTBENH_VAUDP18_SFT);

		/* Enable LO driver bias circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON24,
			RG_AUDLOPWRUP_IBIAS_VAUDP18_MASK_SFT,
			0x1 << RG_AUDLOPWRUP_IBIAS_VAUDP18_SFT);
		/* Enable LO driver core circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON24,
			RG_AUDLOPWRUP_VAUDP18_MASK_SFT,
			0x1 << RG_AUDLOPWRUP_VAUDP18_SFT);

		/* Set LO gain to normal gain step by step */
		regmap_write(priv->regmap, MT6338_ZCD_CON1,
			priv->ana_gain[AUDIO_ANALOG_VOLUME_LINEOUTL]);

		/* Enable AUD_CLK */
		mt6338_set_decoder_clk(priv, true, false);

		/* Switch LOL MUX to audio DAC */
		if (mux == LO_MUX_L_DAC) {
			/* Enable DACL and switch HP MUX to open*/
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
				RG_AUDDACL_PWRUP_VAUDP18_MASK_SFT,
				0x1 << RG_AUDDACL_PWRUP_VAUDP18_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
				RG_AUDDACL_BIAS_PWRUP_VA32_MASK_SFT,
				0x1 << RG_AUDDACL_BIAS_PWRUP_VA32_SFT);

			/* Disable low-noise mode of DAC */
			if (priv->dev_counter[DEVICE_HP] == 0)
				regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x2);
			usleep_range(100, 120);
			/* Switch LOL MUX to DACL */
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON26,
				RG_AUDLOMUXINPUTSEL_VAUDP18_MASK_SFT,
				LO_MUX_L_DAC << RG_AUDLOMUXINPUTSEL_VAUDP18_SFT);

		} else if (mux == LO_MUX_3RD_DAC) {
			/* Enable Audio DAC (3rd DAC) */
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
				RG_AUDDACLO_PWRUP_VAUDP18_MASK_SFT,
				0x1 << RG_AUDDACLO_PWRUP_VAUDP18_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
				RG_AUDDACLO_BIAS_PWRUP_VA32_MASK_SFT,
				0x1 << RG_AUDDACLO_BIAS_PWRUP_VA32_SFT);
			/* Enable low-noise mode of DAC */
			if (priv->dev_counter[DEVICE_HP] == 0)
				regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x1c);
			/* Switch LOL MUX to audio 3rd DAC */
			regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON26,
				RG_AUDLOMUXINPUTSEL_VAUDP18_MASK_SFT,
				LO_MUX_3RD_DAC << RG_AUDLOMUXINPUTSEL_VAUDP18_SFT);
		}
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON36, 0x2);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x7);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON36, 0x0);

		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON33, 0x0);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON34, 0x0);

		/* Select to AVDD30_AUD  */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON43,	0x1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* Switch LOL MUX to open */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON26,
				RG_AUDLOMUXINPUTSEL_VAUDP18_MASK_SFT,
				LO_MUX_OPEN << RG_AUDLOMUXINPUTSEL_VAUDP18_SFT);

		if (mux == LO_MUX_L_DAC) {
			/* Disable Audio DAC */
			regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON0, 0x0);
		/* Disable HP driver core circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
			RG_AUDHPLPWRUP_VAUDP18_MASK_SFT |
			RG_AUDHPRPWRUP_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHPLPWRUP_VAUDP18_SFT);
		/* Disable HP driver bias circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
			RG_AUDHPLPWRUP_IBIAS_VAUDP18_MASK_SFT |
			RG_AUDHPRPWRUP_IBIAS_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHPLPWRUP_IBIAS_VAUDP18_SFT);
		}

		/* Disable Audio DAC */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACLO_PWRUP_VAUDP18_MASK_SFT,
			0x0 << RG_AUDDACLO_PWRUP_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
			RG_AUDDACLO_BIAS_PWRUP_VA32_MASK_SFT,
			0x0 << RG_AUDDACLO_BIAS_PWRUP_VA32_SFT);

		/* Disable AUD_CLK */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON46, 0x0);

		/* decrease LO gain to minimum gain step by step */
		regmap_write(priv->regmap, MT6338_ZCD_CON1,
			     DL_GAIN_N_40DB);

		/* Disable LO driver core circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON24,
				RG_AUDLOPWRUP_VAUDP18_MASK_SFT,
				0x0 << RG_AUDLOPWRUP_VAUDP18_SFT);

		/* Disable LO driver bias circuits */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON24,
				RG_AUDLOPWRUP_IBIAS_VAUDP18_MASK_SFT,
				0x0 << RG_AUDLOPWRUP_IBIAS_VAUDP18_SFT);
		/* 0:normal path */
		regmap_update_bits(priv->regmap, MT6338_AFE_TOP_DEBUG0,
			0x3 << 0x6, 0x0 << 0x6);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_adc_clk_gen_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event 0x%x, vow_enable %d\n",
		 __func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (priv->vow_enable) {
			/* ADC CLK from CLKGEN (3.25MHz) */
			dev_info(priv->dev, "%s(), vow mode\n", __func__);
			/* L */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON12,
					   RG_AUDPREAMPLMODE_MASK_SFT,
					   0x3 << RG_AUDPREAMPLMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
					   RG_AUDADCLMODE_MASK_SFT,
					   0x3 << RG_AUDADCLMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
					   RG_AUDADCL_VOW_MASK_SFT,
					   0x1 << RG_AUDADCL_VOW_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON18,
					   RG_AUDADCLCLKGENMODE_MASK_SFT,
					   0x0 << RG_AUDADCLCLKGENMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON18,
					   RG_AUDADCLCLKSOURCE_MASK_SFT,
					   0x2 << RG_AUDADCLCLKSOURCE_SFT);
			/* R */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON12,
					   RG_AUDPREAMPRMODE_MASK_SFT,
					   0x3 << RG_AUDPREAMPRMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
					   RG_AUDADCRMODE_MASK_SFT,
					   0x3 << RG_AUDADCRMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
					   RG_AUDADCR_VOW_MASK_SFT,
					   0x1 << RG_AUDADCR_VOW_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON19,
					   RG_AUDADCRCLKGENMODE_MASK_SFT,
					   0x0 << RG_AUDADCRCLKGENMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON19,
					   RG_AUDADCRCLKSOURCE_MASK_SFT,
					   0x2 << RG_AUDADCRCLKSOURCE_SFT);
			/* 3 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON13,
					   RG_AUDPREAMP3MODE_MASK_SFT,
					   0x3 << RG_AUDPREAMP3MODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
					   RG_AUDADC3MODE_MASK_SFT,
					   0x3 << RG_AUDADC3MODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
					   RG_AUDADC3_VOW_MASK_SFT,
					   0x1 << RG_AUDADC3_VOW_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON20,
					   RG_AUDADC3CLKGENMODE_MASK_SFT,
					   0x0 << RG_AUDADC3CLKGENMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON20,
					   RG_AUDADC3CLKSOURCE_MASK_SFT,
					   0x2 << RG_AUDADC3CLKSOURCE_SFT);
			/* 4 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON13,
					   RG_AUDPREAMP4MODE_MASK_SFT,
					   0x3 << RG_AUDPREAMP4MODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
					   RG_AUDADC4MODE_MASK_SFT,
					   0x3 << RG_AUDADC4MODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
					   RG_AUDADC4_VOW_MASK_SFT,
					   0x1 << RG_AUDADC4_VOW_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON21,
					   RG_AUDADC4CLKGENMODE_MASK_SFT,
					   0x0 << RG_AUDADC4CLKGENMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON21,
					   RG_AUDADC4CLKSOURCE_MASK_SFT,
					   0x2 << RG_AUDADC4CLKSOURCE_SFT);
		} else {
			if (priv->mic_hifi_mode) {
				/* ADC CLK from CLKGEN (6.5MHz) */
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON12,
						RG_AUDPREAMPLMODE_MASK_SFT,
						0x0 << RG_AUDPREAMPLMODE_SFT);
			} else {
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON12,
						RG_AUDPREAMPLMODE_MASK_SFT,
						0x2 << RG_AUDPREAMPLMODE_SFT);
			}
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON18,
						RG_AUDADCLCLKGENMODE_MASK_SFT |
						RG_AUDADCLCLKSOURCE_MASK_SFT,
						0x5 << RG_AUDADCLCLKSOURCE_SFT);
			/* R */
			if (priv->mic_hifi_mode) {
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON12,
						RG_AUDPREAMPRMODE_MASK_SFT,
						0x0 << RG_AUDPREAMPRMODE_SFT);
			} else {
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON12,
						RG_AUDPREAMPRMODE_MASK_SFT,
						0x2 << RG_AUDPREAMPRMODE_SFT);
			}
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON19,
						RG_AUDADCRCLKGENMODE_MASK_SFT |
						RG_AUDADCRCLKSOURCE_MASK_SFT,
						0x5 << RG_AUDADCRCLKSOURCE_SFT);

			/* ADC3 */
			if (priv->mic_hifi_mode) {
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON13,
						RG_AUDPREAMP3MODE_MASK_SFT,
						0x0 << RG_AUDPREAMP3MODE_SFT);
			} else {
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON13,
						RG_AUDPREAMP3MODE_MASK_SFT,
						0x2 << RG_AUDPREAMP3MODE_SFT);
			}
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON20,
						RG_AUDADC3CLKGENMODE_MASK_SFT |
						RG_AUDADC3CLKSOURCE_MASK_SFT,
						0x5 << RG_AUDADC3CLKSOURCE_SFT);
			/* ADC4 */
			if (priv->mic_hifi_mode) {
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON13,
						RG_AUDPREAMP4MODE_MASK_SFT,
						0x0 << RG_AUDPREAMP4MODE_SFT);
			} else {
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON13,
						RG_AUDPREAMP4MODE_MASK_SFT,
						0x2 << RG_AUDPREAMP4MODE_SFT);
			}
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON21,
						RG_AUDADC4CLKGENMODE_MASK_SFT |
						RG_AUDADC4CLKSOURCE_MASK_SFT,
						0x5 << RG_AUDADC4CLKSOURCE_SFT);
			/* CLKMODE */
			regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON81, 0xf);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* CLKMODE */
		regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON81, 0x0);
		/* L */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON18,
					RG_AUDADCLCLKGENMODE_MASK_SFT |
					RG_AUDADCLCLKSOURCE_MASK_SFT,
					0x0 << RG_AUDADCLCLKSOURCE_SFT);
		/* R */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON19,
					RG_AUDADCRCLKGENMODE_MASK_SFT |
					RG_AUDADCRCLKSOURCE_MASK_SFT,
					0x0 << RG_AUDADCRCLKSOURCE_SFT);
		regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON12, 0x0);
		regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON14, 0x0);
		/* ADC3 */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON20,
					RG_AUDADC3CLKGENMODE_MASK_SFT |
					RG_AUDADC3CLKSOURCE_MASK_SFT,
					0x0 << RG_AUDADC3CLKSOURCE_SFT);
		/* ADC4 */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON21,
					RG_AUDADC4CLKGENMODE_MASK_SFT |
					RG_AUDADC4CLKSOURCE_MASK_SFT,
					0x0 << RG_AUDADC4CLKSOURCE_SFT);
		regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON13, 0x0);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_dcc_clk_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event 0x%x, vow_enable %d\n",
		 __func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* DCC 50k CLK (from 26M) */
		/* MT6338_AFE_DCCLK_CFG0, bit 3 for dm ck swap */
		regmap_write(priv->regmap, MT6338_AFE_DCCLK1_CFG1, 0x20);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK1_CFG0,
			DCCLK1_DIV_L_MASK_SFT,
			0x3 << DCCLK1_DIV_L_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK1_CFG0,
			DCCLK1_PDN_MASK_SFT,
			0x0 << DCCLK1_PDN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK1_CFG0,
			DCCLK1_GEN_ON_MASK_SFT,
			0x1 << DCCLK1_GEN_ON_SFT);
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK1_CFG0,
				DCCLK1_REF_CK_SEL_MASK_SFT,
				0x1 << DCCLK1_REF_CK_SEL_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK1_CFG0,
				DCCLK1_REF_CK_SEL_MASK_SFT,
				0x0 << DCCLK1_REF_CK_SEL_SFT);
		}
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK1_CFG2,
			DCCLK1_RESYNC_BYPASS_MASK_SFT,
			0x1 << DCCLK1_RESYNC_BYPASS_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK1_CFG0,
			DCCLK1_PDN_MASK_SFT,
			0x1 << DCCLK1_PDN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK1_CFG0,
			DCCLK1_GEN_ON_MASK_SFT,
			0x0 << DCCLK1_GEN_ON_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_r_dcc_clk_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event 0x%x, vow_enable %d\n",
		 __func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* DCC 50k CLK (from 26M) */
		/* MT6338_AFE_DCCLK_CFG0, bit 3 for dm ck swap */
		regmap_write(priv->regmap, MT6338_AFE_DCCLK2_CFG1, 0x20);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK2_CFG0,
			DCCLK2_DIV_L_MASK_SFT,
			0x3 << DCCLK2_DIV_L_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK2_CFG0,
			DCCLK2_PDN_MASK_SFT,
			0x0 << DCCLK2_PDN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK2_CFG0,
			DCCLK2_GEN_ON_MASK_SFT,
			0x1 << DCCLK2_GEN_ON_SFT);
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK2_CFG0,
				DCCLK2_REF_CK_SEL_MASK_SFT,
				0x1 << DCCLK2_REF_CK_SEL_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK2_CFG0,
				DCCLK2_REF_CK_SEL_MASK_SFT,
				0x0 << DCCLK2_REF_CK_SEL_SFT);
		}
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK2_CFG2,
			DCCLK2_RESYNC_BYPASS_MASK_SFT,
			0x1 << DCCLK2_RESYNC_BYPASS_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK2_CFG0,
			DCCLK2_PDN_MASK_SFT,
			0x1 << DCCLK2_PDN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK2_CFG0,
			DCCLK2_GEN_ON_MASK_SFT,
			0x0 << DCCLK2_GEN_ON_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_3_dcc_clk_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event 0x%x, vow_enable %d\n",
		 __func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* DCC 50k CLK (from 26M) */
		/* MT6338_AFE_DCCLK_CFG0, bit 3 for dm ck swap */
		regmap_write(priv->regmap, MT6338_AFE_DCCLK3_CFG1, 0x20);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK3_CFG0,
			DCCLK3_DIV_L_MASK_SFT,
			0x3 << DCCLK3_DIV_L_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK3_CFG0,
			DCCLK3_PDN_MASK_SFT,
			0x0 << DCCLK3_PDN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK3_CFG0,
			DCCLK3_GEN_ON_MASK_SFT,
			0x1 << DCCLK3_GEN_ON_SFT);
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK3_CFG0,
				DCCLK3_REF_CK_SEL_MASK_SFT,
				0x1 << DCCLK3_REF_CK_SEL_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK3_CFG0,
				DCCLK3_REF_CK_SEL_MASK_SFT,
				0x0 << DCCLK3_REF_CK_SEL_SFT);
		}
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK3_CFG2,
			DCCLK3_RESYNC_BYPASS_MASK_SFT,
			0x1 << DCCLK3_RESYNC_BYPASS_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK3_CFG0,
			DCCLK3_PDN_MASK_SFT,
			0x1 << DCCLK3_PDN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK3_CFG0,
			DCCLK3_GEN_ON_MASK_SFT,
			0x0 << DCCLK3_GEN_ON_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_4_dcc_clk_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event 0x%x, vow_enable %d\n",
		 __func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* DCC 50k CLK (from 26M) */
		/* MT6338_AFE_DCCLK_CFG0, bit 3 for dm ck swap */
		regmap_write(priv->regmap, MT6338_AFE_DCCLK4_CFG1, 0x20);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK4_CFG0,
			DCCLK4_DIV_L_MASK_SFT,
			0x3 << DCCLK4_DIV_L_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK4_CFG0,
			DCCLK4_PDN_MASK_SFT,
			0x0 << DCCLK4_PDN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK4_CFG0,
			DCCLK4_GEN_ON_MASK_SFT,
			0x1 << DCCLK4_GEN_ON_SFT);
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK4_CFG0,
				DCCLK4_REF_CK_SEL_MASK_SFT,
				0x1 << DCCLK4_REF_CK_SEL_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK4_CFG0,
				DCCLK4_REF_CK_SEL_MASK_SFT,
				0x0 << DCCLK4_REF_CK_SEL_SFT);
		}
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK4_CFG2,
			DCCLK4_RESYNC_BYPASS_MASK_SFT,
			0x1 << DCCLK4_RESYNC_BYPASS_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK4_CFG0,
			DCCLK4_PDN_MASK_SFT,
			0x1 << DCCLK4_PDN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_DCCLK4_CFG0,
			DCCLK4_GEN_ON_MASK_SFT,
			0x0 << DCCLK4_GEN_ON_SFT);
		break;
	default:
		break;
	}
	return 0;
}

static int mt_mic_bias_0_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE_0];

	dev_info(priv->dev, "%s(), event 0x%x, mic_type %d, vow_enable: %d\n",
		 __func__, event, mic_type, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		switch (mic_type) {
		case MIC_TYPE_MUX_DCC_ECM_DIFF:
			regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON60, 0x77);
			break;
		case MIC_TYPE_MUX_DCC_ECM_SINGLE:
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON60,
				RG_AUDMICBIAS0DCSW0P1EN_MASK_SFT,
				0x1 << RG_AUDMICBIAS0DCSW0P1EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON60,
				RG_AUDMICBIAS0DCSW2P1EN_MASK_SFT,
				0x1 << RG_AUDMICBIAS0DCSW2P1EN_SFT);
			break;
		default:
			break;
		}

		/* MISBIAS0 = 1P9V */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON59,
			RG_AUDMICBIAS0VREF_MASK_SFT,
			MIC_BIAS_1P9 << RG_AUDMICBIAS0VREF_SFT);
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON59,
				RG_AUDMICBIAS0LOWPEN_MASK_SFT,
				0x1 << RG_AUDMICBIAS0LOWPEN_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON59,
				RG_AUDMICBIAS0LOWPEN_MASK_SFT,
				0x0 << RG_AUDMICBIAS0LOWPEN_SFT);
		}
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON59,
			RG_AUDMICBIAS0ULTRALOWPEN_MASK_SFT,
			0x0 << RG_AUDMICBIAS0ULTRALOWPEN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON68,
			RG_AUDACCDETMICBIAS0PULLLOW_MASK_SFT,
			0x1 << RG_AUDACCDETMICBIAS0PULLLOW_SFT);
		regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON53, 0x10);
		if (priv->mic_hifi_mode)
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON17,
				RG_AUDRADCFLASHIDDTEST_MASK_SFT,
				0x1 << RG_AUDRADCFLASHIDDTEST_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Disable MICBIAS0, MISBIAS0 = 1P7V */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON59,
			RG_AUDMICBIAS0VREF_MASK_SFT,
			MIC_BIAS_1P7 << RG_AUDMICBIAS0VREF_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON59,
			RG_AUDMICBIAS0LOWPEN_MASK_SFT,
			0x0 << RG_AUDMICBIAS0LOWPEN_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_mic_bias_1_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE_1];

	dev_info(priv->dev, "%s(), event 0x%x, mic_type %d, vow_enable: %d\n",
		 __func__, event, mic_type, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* MIC_TYPE_MUX_DCC_ECM_SINGLE */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON62,
			RG_AUDMICBIAS1DCSW1PEN_MASK_SFT,
			0x0 << RG_AUDMICBIAS1DCSW1PEN_SFT);
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON61,
				RG_AUDMICBIAS1LOWPEN_MASK_SFT,
				0x1 << RG_AUDMICBIAS1LOWPEN_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON61,
				RG_AUDMICBIAS1LOWPEN_MASK_SFT,
				0x0 << RG_AUDMICBIAS1LOWPEN_SFT);
		}
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON61,
			RG_AUDMICBIAS1ULTRALOWPEN_MASK_SFT,
			0x0 << RG_AUDMICBIAS1ULTRALOWPEN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON68,
			RG_AUDACCDETMICBIAS1PULLLOW_MASK_SFT,
			0x1 << RG_AUDACCDETMICBIAS1PULLLOW_SFT);
		regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON53, 0x10);
		if (priv->mic_hifi_mode)
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON17,
				RG_AUDRADCFLASHIDDTEST_MASK_SFT,
				0x1 << RG_AUDRADCFLASHIDDTEST_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON61,
			RG_AUDMICBIAS1LOWPEN_MASK_SFT,
			0x1 << RG_AUDMICBIAS1LOWPEN_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_mic_bias_2_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE_2];

	dev_info(priv->dev, "%s(), event 0x%x, mic_type %d, vow_enable: %d\n",
		 __func__, event, mic_type, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		switch (mic_type) {
		case MIC_TYPE_MUX_DCC_ECM_DIFF:
			regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON64, 0x77);
			break;
		case MIC_TYPE_MUX_DCC_ECM_SINGLE:
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON64,
				RG_AUDMICBIAS2DCSW0P1EN_MASK_SFT,
				0x1 << RG_AUDMICBIAS2DCSW0P1EN_SFT);
			break;
		default:
			break;
		}

		/* MISBIAS2 = 1P9V */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON63,
			RG_AUDMICBIAS2VREF_MASK_SFT,
			MIC_BIAS_1P9 << RG_AUDMICBIAS2VREF_SFT);
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON63,
				RG_AUDMICBIAS2LOWPEN_MASK_SFT,
				0x1 << RG_AUDMICBIAS2LOWPEN_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON63,
				RG_AUDMICBIAS2LOWPEN_MASK_SFT,
				0x0 << RG_AUDMICBIAS2LOWPEN_SFT);
		}
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON63,
			RG_AUDMICBIAS2ULTRALOWPEN_MASK_SFT,
			0x0 << RG_AUDMICBIAS2ULTRALOWPEN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON68,
			RG_AUDACCDETMICBIAS2PULLLOW_MASK_SFT,
			0x1 << RG_AUDACCDETMICBIAS2PULLLOW_SFT);
		regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON53, 0x10);
		if (priv->mic_hifi_mode)
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON17,
				RG_AUDRADCFLASHIDDTEST_MASK_SFT,
				0x1 << RG_AUDRADCFLASHIDDTEST_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Disable MICBIAS2, MISBIAS0 = 1P7V */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON63,
			RG_AUDMICBIAS2VREF_MASK_SFT,
			MIC_BIAS_1P7 << RG_AUDMICBIAS2VREF_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON63,
			RG_AUDMICBIAS2LOWPEN_MASK_SFT,
			0x0 << RG_AUDMICBIAS2LOWPEN_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_mic_bias_3_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mic_type = priv->mux_select[MUX_MIC_TYPE_3];

	dev_info(priv->dev, "%s(), event 0x%x, mic_type %d, vow_enable: %d\n",
		 __func__, event, mic_type, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		switch (mic_type) {
		case MIC_TYPE_MUX_DCC_ECM_DIFF:
			regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON66, 0x77);
			break;
		case MIC_TYPE_MUX_DCC_ECM_SINGLE:
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON66,
				RG_AUDMICBIAS3DCSW3P1EN_MASK_SFT,
				0x1 << RG_AUDMICBIAS3DCSW3P1EN_SFT);
			break;
		default:
			break;
		}

		/* MISBIAS3 = 1P9V */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON65,
			RG_AUDMICBIAS3VREF_MASK_SFT,
			MIC_BIAS_1P9 << RG_AUDMICBIAS3VREF_SFT);
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON65,
				RG_AUDMICBIAS3LOWPEN_MASK_SFT,
				0x1 << RG_AUDMICBIAS3LOWPEN_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON65,
				RG_AUDMICBIAS3LOWPEN_MASK_SFT,
				0x0 << RG_AUDMICBIAS3LOWPEN_SFT);
		}
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON65,
			RG_AUDMICBIAS3ULTRALOWPEN_MASK_SFT,
			0x0 << RG_AUDMICBIAS3ULTRALOWPEN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON69,
			RG_AUDACCDETMICBIAS3PULLLOW_MASK_SFT,
			0x1 << RG_AUDACCDETMICBIAS3PULLLOW_SFT);
		regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON53, 0x10);
		if (priv->mic_hifi_mode)
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON17,
				RG_AUDRADCFLASHIDDTEST_MASK_SFT,
				0x1 << RG_AUDRADCFLASHIDDTEST_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Disable MICBIAS3 = 1P7V */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON65,
			RG_AUDMICBIAS3VREF_MASK_SFT,
			MIC_BIAS_1P7 << RG_AUDMICBIAS3VREF_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON65,
			RG_AUDMICBIAS3LOWPEN_MASK_SFT,
			0x0 << RG_AUDMICBIAS3LOWPEN_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_sram_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* audio clk source from internal dcxo */
		regmap_write(priv->regmap, MT6338_AUD_TOP_SRAM_CON, 0x0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* audio clk source from internal dcxo */
		regmap_write(priv->regmap, MT6338_AUD_TOP_SRAM_CON, 0x6);

		break;
	default:
		break;
	}

	return 0;
}

static int mt_clksq_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* audio clk source from internal dcxo */
		regmap_write(priv->regmap, MT6338_CLKSQ_PMU_CON0, 0xe);

		/* NLE enable */
		if (priv->hp_hifi_mode == 2) {
			regmap_write(priv->regmap, MT6338_AFE_NLE_CFG_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_CFG_H, 0x80);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_write(priv->regmap, MT6338_CLKSQ_PMU_CON0, 0x0);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_key_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		keylock_reset(priv);
		break;
	case SND_SOC_DAPM_POST_PMD:
		keylock_set(priv);
		break;
	default:
		break;
	}

	return 0;
}
static int mt_dcxo_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* audio clk source from internal dcxo */
		mt6338_set_dcxo(priv, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6338_set_dcxo(priv, false);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_vaud18_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* audio clk source from internal dcxo */
		regmap_update_bits(priv->regmap, MT6338_MTC_CTL0,
			RG_CO_OP_EN_MASK_SFT,
			0x1 << RG_CO_OP_EN_SFT);
		regmap_update_bits(priv->regmap, MT6338_DA_INTF_STTING3,
			RG_HWMD_CO_CTL_MASK_SFT,
			0x0 << RG_HWMD_CO_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_CON2,
			RG_LDO_VAUD18_CK_SW_MODE_MASK_SFT,
			0x0 << RG_LDO_VAUD18_CK_SW_MODE_SFT);
		regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_OP_EN0_SET,
			RG_LDO_VAUD18_HW0_OP_EN_MASK_SFT,
			0x1 << RG_LDO_VAUD18_HW0_OP_EN_SFT);
		regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_OP_CFG0_SET,
			RG_LDO_VAUD18_HW0_OP_CFG_MASK_SFT,
			0x1 << RG_LDO_VAUD18_HW0_OP_CFG_SFT);
		regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_OP_EN1_SET,
			RG_LDO_VAUD18_HW14_OP_EN_MASK_SFT,
			0x1 << RG_LDO_VAUD18_HW14_OP_EN_SFT);
		regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_OP_CFG1_SET,
			RG_LDO_VAUD18_HW8_OP_CFG_MASK_SFT,
			0x1 << RG_LDO_VAUD18_SW_OP_CFG_SFT);
		regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_CON0,
			RG_LDO_VAUD18_EN_0_MASK_SFT,
			0x1 << RG_LDO_VAUD18_EN_0_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_CON0,
			RG_LDO_VAUD18_EN_0_MASK_SFT,
			0x0 << RG_LDO_VAUD18_EN_0_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int is_need_pll_208M(struct snd_soc_dapm_widget *source,
			       struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	bool ret = false;
	int i = 0;

	for (i = 0; i < MT6338_AIF_NUM; i++) {
		if (priv->dl_rate[i] > 48000 || priv->ul_rate[i] > 48000) {
			ret = true;
			break;
		}
	}

	return (ret) ? 1 : 0;
}

static int is_hp_lowpower(struct snd_soc_dapm_widget *source,
			       struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	return (priv->hp_hifi_mode == 0) ? 1 : 0;
}

#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
static int mt_pll208m_vow_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(priv->regmap, MT6338_VPLL18_PMU_CON0,
			RG_VPLL18_LDO_VOWPLL_EN_VA18_MASK_SFT,
			0x1 << RG_VPLL18_LDO_VOWPLL_EN_VA18_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_VPLL18_PMU_CON0,
			RG_VPLL18_LDO_VOWPLL_EN_VA18_MASK_SFT,
			0x0 << RG_VPLL18_LDO_VOWPLL_EN_VA18_SFT);
		break;
	default:
		break;
	}
	return 0;
}

static int mt_vowpll_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol,
			   int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_write(priv->regmap, MT6338_VOWPLL_PMU_CON4, 0x1E);
		regmap_write(priv->regmap, MT6338_VOWPLL_PMU_CON5, 0x03);
		regmap_write(priv->regmap, MT6338_VOWPLL_PMU_CON1, 0x4A);
		regmap_write(priv->regmap, MT6338_VOWPLL_PMU_CON3, 0x60);
		break;
	default:
		break;
	}
	return 0;
}

static int mt_vow_digital_cfg_event(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
			RG_AUDADCL_VOW_MASK_SFT,
			0x1 << RG_AUDADCL_VOW_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
			RG_AUDADCR_VOW_MASK_SFT,
			0x1 << RG_AUDADCR_VOW_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
			RG_AUDADC3_VOW_MASK_SFT,
			0x1 << RG_AUDADC3_VOW_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
			RG_AUDADC4_VOW_MASK_SFT,
			0x1 << RG_AUDADC4_VOW_SFT);
		regmap_write(priv->regmap, MT6338_TOP_CKPDN_CON1_CLR,
			0x1 << RG_AUD_13M_CK_PDN_SFT);
		regmap_write(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H_CLR,
			0x1 << RG_VOW13M_CK_PDN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_TOP_CON0,
			VOW_DMIC_CK_SEL_MASK_SFT,
			0x0 << VOW_DMIC_CK_SEL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_TOP_CON2,
			SAMPLE_BASE_MODE_CH1_MASK_SFT,
			0x1 << SAMPLE_BASE_MODE_CH1_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_TOP_CON3,
			VOW_INTR_SOURCE_SEL_CH1_MASK_SFT,
			0x1 << VOW_INTR_SOURCE_SEL_CH1_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_TOP_CON4,
			SAMPLE_BASE_MODE_CH2_MASK_SFT,
			0x1 << SAMPLE_BASE_MODE_CH2_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_TOP_CON5,
			VOW_INTR_SOURCE_SEL_CH2_MASK_SFT,
			0x1 << VOW_INTR_SOURCE_SEL_CH2_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_TOP_CON6,
			SAMPLE_BASE_MODE_CH3_MASK_SFT,
			0x1 << SAMPLE_BASE_MODE_CH3_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_TOP_CON7,
			VOW_INTR_SOURCE_SEL_CH3_MASK_SFT,
			0x1 << VOW_INTR_SOURCE_SEL_CH3_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_TOP_CON8,
			SAMPLE_BASE_MODE_CH4_MASK_SFT,
			0x1 << SAMPLE_BASE_MODE_CH4_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_TOP_CON9,
			VOW_INTR_SOURCE_SEL_CH4_MASK_SFT,
			0x1 << VOW_INTR_SOURCE_SEL_CH4_SFT);

		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG1,
			     priv->reg_afe_vow_vad_cfg0 & 0xff);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG0,
			     priv->reg_afe_vow_vad_cfg0 >> 8);

		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG9,
			     priv->reg_afe_vow_vad_cfg1 & 0xff);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG8,
			     priv->reg_afe_vow_vad_cfg1 >> 8);

		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG17,
			     priv->reg_afe_vow_vad_cfg2 & 0xff);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG16,
			     priv->reg_afe_vow_vad_cfg2 >> 8);

		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG25,
			     priv->reg_afe_vow_vad_cfg3 & 0xff);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG24,
			     priv->reg_afe_vow_vad_cfg3 >> 8);

		regmap_update_bits(priv->regmap, MT6338_AFE_VOW_VAD_CFG48,
				 K_GAMMA_CH1_MASK_SFT,
				 priv->reg_afe_vow_vad_cfg4
				 << K_GAMMA_CH1_SFT);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG33,
			     priv->reg_afe_vow_vad_cfg5 & 0xff);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG32,
			     priv->reg_afe_vow_vad_cfg5 >> 8);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG50, 0x7f);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG51, 0x00);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG52, 0x00);
		regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG53, 0x00);
		if (priv->vow_channel == 2) {
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG3,
				     priv->reg_afe_vow_vad_cfg0 & 0xff);
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG2,
				     priv->reg_afe_vow_vad_cfg0 >> 8);

			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG11,
				     priv->reg_afe_vow_vad_cfg1 & 0xff);
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG10,
				     priv->reg_afe_vow_vad_cfg1 >> 8);

			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG19,
				     priv->reg_afe_vow_vad_cfg2 & 0xff);
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG18,
				     priv->reg_afe_vow_vad_cfg2 >> 8);

			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG27,
				     priv->reg_afe_vow_vad_cfg3 & 0xff);
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG26,
				     priv->reg_afe_vow_vad_cfg3 >> 8);

			regmap_update_bits(priv->regmap, MT6338_AFE_VOW_VAD_CFG48,
					   K_GAMMA_CH2_MASK_SFT,
					   priv->reg_afe_vow_vad_cfg4
					   << K_GAMMA_CH2_SFT);

			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG35,
				     priv->reg_afe_vow_vad_cfg5 & 0xff);
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG34,
				     priv->reg_afe_vow_vad_cfg5 >> 8);
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG54, 0x7f);
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG55, 0x00);
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG56, 0x00);
			regmap_write(priv->regmap, MT6338_AFE_VOW_VAD_CFG57, 0x00);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		regmap_write(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H_SET,
			0x1 << RG_VOW13M_CK_PDN_SFT);
		regmap_write(priv->regmap, MT6338_TOP_CKPDN_CON1_SET,
			0x1 << RG_AUD_13M_CK_PDN_SFT);
		break;
	default:
		break;
	}
	return 0;
}
#endif

static int mt_aud208_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H,
			RG_AUD208M_CK_PDN_MASK_SFT,
			0x0 << RG_AUD208M_CK_PDN_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H,
			RG_AUD208M_CK_PDN_MASK_SFT,
			0x1 << RG_AUD208M_CK_PDN_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_vpll18_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n",
			 __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(priv->regmap, MT6338_VPLL18_PMU_CON0,
			RG_VPLL18_LDO_VREF_EN_VA32_MASK_SFT,
			0x1 << RG_VPLL18_LDO_VREF_EN_VA32_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_VPLL18_PMU_CON0,
			RG_VPLL18_LDO_VREF_EN_VA32_MASK_SFT,
			0x0 << RG_VPLL18_LDO_VREF_EN_VA32_SFT);
		break;
	default:
		break;
	}
	return 0;
}

static int mt_pll208m_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* LDO */
		regmap_update_bits(priv->regmap, MT6338_VPLL18_PMU_CON0,
			RG_VPLL18_LDO_PLL208M_EN_VA18_MASK_SFT,
			0x1 << RG_VPLL18_LDO_PLL208M_EN_VA18_SFT);

		/* regmap_write(priv->regmap, MT6338_VPLL18_PMU_CON2, 0x1);*/
		/* PSC */
		regmap_update_bits(priv->regmap, MT6338_PLL208M_PMU_CON0,
			RG_PLL208M_SDM_PCW_CHG_MASK_SFT,
			0x1 << RG_PLL208M_SDM_PCW_CHG_SFT);
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON1, 0x0);
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON2, 0x0);
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON3, 0x80);

		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON5, 0x2);
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON4, 0x17);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* PLL208M */
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON4, 0x16);
		regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON5, 0x1);

		/* VPLL18 LDO */
		regmap_update_bits(priv->regmap, MT6338_VPLL18_PMU_CON0,
			RG_VPLL18_LDO_PLL208M_EN_VA18_MASK_SFT,
			0x0 << RG_VPLL18_LDO_PLL208M_EN_VA18_SFT);

		break;
	default:
		break;
	}

	return 0;
}

static int mt_audtop_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON1, 0x0);
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON2, 0x0);
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON3, 0x0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON3, 0x2);
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON2, 0xff);
		regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON1, 0xbf);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_sdm_fifo_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->mux_select[MUX_HP_L] == HP_MUX_HPSPK) {
			/* todo :HPSPK Setting */
			dev_dbg(priv->dev, "%s(), HP_MUX_HPSPK\n", __func__);
		}
		regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON2_L, 0x6);
		/* scrambler clock on enable */
		regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON0_H, 0xcb);
		/* scrambler clock on enable */
		regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON0_L, 0xa0);
		/* scrambler enable */
		regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON2_H, 0x70);
		regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON2_L, 0xb);

		/* 2ND DL sdm audio fifo clock power on */
		regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON11_L, 0x6);
		/* 2ND DL scrambler clock on enable */
		regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON9_H, 0xcb);
		/* 2ND DL scrambler clock on enable */
		regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON9_L,
			CCI_SPLT_SCRMB_ON_2ND_MASK_SFT,
			0x1 << CCI_SPLT_SCRMB_ON_2ND_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON9_L,
			CCI_ZERO_PAD_DISABLE_2ND_MASK_SFT,
			0x1 << CCI_ZERO_PAD_DISABLE_2ND_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON9_L,
			CCI_AUD_SDM_7BIT_SEL_2ND_MASK_SFT,
			0x0 << CCI_AUD_SDM_7BIT_SEL_2ND_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON9_L,
			CCI_SCRAMBLER_EN_2ND_MASK_SFT,
			0x1 << CCI_SCRAMBLER_EN_2ND_SFT);
		/* 2ND DL sdm power on2ND DL sdm power on */
		regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON11_L, 0xb);
		break;
	case SND_SOC_DAPM_POST_PMD:
			/* scrambler disable */
			regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON0_L, 0x20);
			/* scrambler clock on disable */
			regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON0_H, 0xca);
			/* sdm audio fifo clock power off */
			regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON2_L, 0x9);
			/* 2ND DL scrambler disable */
			regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON9_L, 0x20);
			/* 2ND DL scrambler clock on disable */
			regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON9_H, 0xca);
			/* 2ND DL sdm power off */
			regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON11_L, 0x9);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_mtkaif_tx_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6338_mtkaif_tx_enable(priv);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6338_mtkaif_tx_disable(priv);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_ul_src_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6338_set_ul_src(priv, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6338_set_ul_src(priv, false);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_ul34_src_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6338_set_ul34_src(priv, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6338_set_ul34_src(priv, false);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_ul_src_dmic_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rate;

	if (priv->ul_rate[0] != 0)
		rate = mt6338_voice_rate_transform(priv->ul_rate[0]);
	else
		rate = MT6338_VOICE_48000HZ;

	dev_info(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->dmic_one_wire_mode) {
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_3,
				ADDA_UL_DMIC_PHASE_SEL_CH1_MASK_SFT,
				0x0 << ADDA_UL_DMIC_PHASE_SEL_CH1_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_3,
				ADDA_UL_DMIC_PHASE_SEL_CH2_MASK_SFT,
				0x4 << ADDA_UL_DMIC_PHASE_SEL_CH2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_2,
				MT6338_ADDA_UL_TWO_WIRE_MODE_CTL_MASK_SFT,
				0x0 << MT6338_ADDA_UL_TWO_WIRE_MODE_CTL_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_3,
				ADDA_UL_DMIC_PHASE_SEL_CH1_MASK_SFT,
				0x0 << ADDA_UL_DMIC_PHASE_SEL_CH1_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_3,
				ADDA_UL_DMIC_PHASE_SEL_CH2_MASK_SFT,
				0x0 << ADDA_UL_DMIC_PHASE_SEL_CH2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_2,
				MT6338_ADDA_UL_TWO_WIRE_MODE_CTL_MASK_SFT,
				0x1 << MT6338_ADDA_UL_TWO_WIRE_MODE_CTL_SFT);
		}
		/* UL mode setting: AMIC/DMIC mode, voice_mode, loopback_mode*/
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_2,
			MT6338_ADDA_UL_MODE_3P25M_CH1_CTL_MASK_SFT,
			0x1 << MT6338_ADDA_UL_MODE_3P25M_CH1_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_2,
			MT6338_ADDA_UL_MODE_3P25M_CH2_CTL_MASK_SFT,
			0x1 << MT6338_ADDA_UL_MODE_3P25M_CH2_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_2,
			MT6338_ADDA_UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
			rate << MT6338_ADDA_UL_VOICE_MODE_CH1_CH2_CTL_SFT);
		/* iir on */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_1,
			ADDA_UL_IIRMODE_CTL_MASK_SFT,
			0x0 << ADDA_UL_IIRMODE_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_1,
			ADDA_UL_IIR_ON_TMP_CTL_MASK_SFT,
			0x0 << ADDA_UL_IIR_ON_TMP_CTL_SFT);
		/* MT6338_PMIC_ULSRC_DMIC_3P25M */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0,
			ADDA_DIGMIC_3P25M_1P625M_SEL_CTL_MASK_SFT,
			0x0 << ADDA_DIGMIC_3P25M_1P625M_SEL_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0,
			ADDA_DIGMIC_4P33M_SEL_MASK_SFT,
			0x0 << ADDA_DIGMIC_4P33M_SEL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_1,
			ADDA_DMIC_LOW_POWER_MODE_CTL_MASK_SFT,
			0x0 << ADDA_DMIC_LOW_POWER_MODE_CTL_SFT);
		/* [0] ul_src_on = 1 */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0,
			ADDA_UL_SRC_ON_TMP_CTL_MASK_SFT,
			0x1 << ADDA_UL_SRC_ON_TMP_CTL_SFT);
		/* Dmic enable */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON55,
			RG_AUDDIGMICBIAS_MASK_SFT,
			0x2 << RG_AUDDIGMICBIAS_SFT);
		/* Dmic Driving */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON55,
			RG_AUDDIGMIC_DRVA_EN_MASK_SFT,
			0x1 << RG_AUDDIGMIC_DRVA_EN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON55,
			RG_AUDDIGMIC_DRVB_EN_MASK_SFT,
			0x1 << RG_AUDDIGMIC_DRVB_EN_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Dmic disable */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON55,
			RG_AUDDIGMICBIAS_MASK_SFT,
			0x0 << RG_AUDDIGMICBIAS_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0,
			ADDA_UL_SRC_ON_TMP_CTL_MASK_SFT,
			0x0 << ADDA_UL_SRC_ON_TMP_CTL_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_ul_src_34_dmic_event(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol,
				   int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rate;

	if (priv->ul_rate[0] != 0)
		rate = mt6338_voice_rate_transform(priv->ul_rate[0]);
	else
		rate = MT6338_VOICE_48000HZ;

	dev_info(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->dmic_one_wire_mode) {
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_3,
				ADDA6_UL_DMIC_PHASE_SEL_CH1_MASK_SFT,
				0x0 << ADDA6_UL_DMIC_PHASE_SEL_CH1_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_3,
				ADDA6_UL_DMIC_PHASE_SEL_CH2_MASK_SFT,
				0x4 << ADDA6_UL_DMIC_PHASE_SEL_CH2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_2,
				ADDA6_UL_TWO_WIRE_MODE_CTL_MASK_SFT,
				0x0 << ADDA6_UL_TWO_WIRE_MODE_CTL_SFT);
		} else {
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_3,
				ADDA6_UL_DMIC_PHASE_SEL_CH1_MASK_SFT,
				0x0 << ADDA6_UL_DMIC_PHASE_SEL_CH1_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_3,
				ADDA6_UL_DMIC_PHASE_SEL_CH2_MASK_SFT,
				0x0 << ADDA6_UL_DMIC_PHASE_SEL_CH2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_2,
				ADDA6_UL_TWO_WIRE_MODE_CTL_MASK_SFT,
				0x1 << ADDA6_UL_TWO_WIRE_MODE_CTL_SFT);
		}
		/* UL mode setting: AMIC/DMIC mode, voice_mode, loopback_mode*/
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_2,
			ADDA6_UL_MODE_3P25M_CH1_CTL_MASK_SFT,
			0x1 << ADDA6_UL_MODE_3P25M_CH1_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_2,
			ADDA6_UL_MODE_3P25M_CH2_CTL_MASK_SFT,
			0x1 << ADDA6_UL_MODE_3P25M_CH2_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_2,
			ADDA6_UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
			rate << ADDA6_UL_VOICE_MODE_CH1_CH2_CTL_SFT);
		/* iir on */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_1,
			ADDA6_UL_IIRMODE_CTL_MASK_SFT,
			0x0 << ADDA6_UL_IIRMODE_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_1,
			ADDA6_UL_IIR_ON_TMP_CTL_MASK_SFT,
			0x0 << ADDA6_UL_IIR_ON_TMP_CTL_SFT);
		/* MT6338_PMIC_ULSRC_DMIC_3P25M */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0,
			ADDA6_DIGMIC_3P25M_1P625M_SEL_CTL_MASK_SFT,
			0x0 << ADDA6_DIGMIC_3P25M_1P625M_SEL_CTL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0,
			ADDA6_DIGMIC_4P33M_SEL_MASK_SFT,
			0x0 << ADDA6_DIGMIC_4P33M_SEL_SFT);
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_1,
			ADDA6_DMIC_LOW_POWER_MODE_CTL_MASK_SFT,
			0x0 << ADDA6_DMIC_LOW_POWER_MODE_CTL_SFT);
		/* [0] ul_src_on = 1 */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0,
			ADDA6_UL_SRC_ON_TMP_CTL_MASK_SFT,
			0x1 << ADDA6_UL_SRC_ON_TMP_CTL_SFT);
		/* Dmic 1 enable */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON57,
			RG_AUDDIGMICBIAS1_MASK_SFT,
			0x2 << RG_AUDDIGMICBIAS1_SFT);
		/* Dmic Driving */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON57,
			RG_AUDDIGMIC1_DRVA_EN_MASK_SFT,
			0x1 << RG_AUDDIGMIC1_DRVA_EN_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON57,
			RG_AUDDIGMIC1_DRVB_EN_MASK_SFT,
			0x1 << RG_AUDDIGMIC1_DRVB_EN_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Dmic 1 disable */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON57,
			RG_AUDDIGMICBIAS1_MASK_SFT,
			0x0 << RG_AUDDIGMICBIAS1_SFT);
		/* [0] ul_src_on = 1 */
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0,
			ADDA6_UL_SRC_ON_TMP_CTL_MASK_SFT,
			0x0 << ADDA6_UL_SRC_ON_TMP_CTL_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int is_need_ulcf(struct snd_soc_dapm_widget *source,
			       struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	return (priv->mic_ulcf_en) ? 1 : 0;
}

static int mt_ulcf_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6338_set_ulcf(priv, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6338_set_ulcf(priv, false);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_adc_l_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rc_tune = 0;
	unsigned int value = 0;

	dev_info(priv->dev, "%s(), event = 0x%x, vow_enable %d\n",
		 __func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
				RG_AUDADCLMODE_MASK_SFT,
				0x2 << RG_AUDADCLMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON24,
				RG_AUDADCWIDECM_MASK_SFT,
				0x1 << RG_AUDADCWIDECM_SFT);
			/* ADC L input resistor value selection
			 * (10000)   8kohm (01000)  16kohm (00100)  32kohm
			 * (00010)  64kohm (00001) 128kohm (00000) 256kohm
			 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
				RG_AUDADCLRINOHM_MASK_SFT,
				0x4 << RG_AUDADCLRINOHM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_0,
				RG_VCM_PGA_LPM_SEL_MASK_SFT,
				0x9 << RG_VCM_PGA_LPM_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_0,
				RG_VCM_PGA_HIFI_SEL_MASK_SFT,
				0x8 << RG_VCM_PGA_HIFI_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUDLADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUDLADCFLASHFFCAPVREF_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCLFLASHVREFRES_LPM_MASK_SFT,
				0x1 << RG_AUDADCLFLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCLFLASHVREFRES_LPM2_MASK_SFT,
				0x0 << RG_AUDADCLFLASHVREFRES_LPM2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON28,
				RG_AUDRCTUNEL_MASK_SFT,
				0x4 << RG_AUDRCTUNEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON28,
				RG_AUDRCTUNELSEL_MASK_SFT,
				0x0 << RG_AUDRCTUNELSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON18,
				RG_AUDADCLCLKSEL_MASK_SFT,
				0x3 << RG_AUDADCLCLKSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDLADCHALFCLK_MASK_SFT,
				0x0 << RG_AUDLADCHALFCLK_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDLADCDACMODE_SEL_MASK_SFT,
				0x1 << RG_AUDLADCDACMODE_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON8,
				RG_AUDADC1STSTAGELPEN_MASK_SFT,
				0x1 << RG_AUDADC1STSTAGELPEN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON52,
				RG_AUDLADC1STSTAGELPEN_0_MASK_SFT,
				0x1 << RG_AUDLADC1STSTAGELPEN_0_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
				RG_AUDADCLMODE_MASK_SFT,
				0x2 << RG_AUDADCLMODE_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (priv->vow_enable) {
			usleep_range(500, 520);
			/* Read 5-bit audio L RC tune data */
			regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON32, &rc_tune);
			dev_dbg(priv->dev, "%s(), vow rc_tune %d\n",
				 __func__, rc_tune);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON1,
				RG_AUDADCLPWRUP_MASK_SFT,
				0x0 << RG_AUDADCLPWRUP_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON24,
				RG_AUDADCWIDECM_MASK_SFT,
				0x1 << RG_AUDADCWIDECM_SFT);
			/* ADC L input resistor value selection
			 * (10000)   8kohm (01000)  16kohm (00100)  32kohm
			 * (00010)  64kohm (00001) 128kohm (00000) 256kohm
			 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
				RG_AUDADCLRINOHM_MASK_SFT,
				0x8 << RG_AUDADCLRINOHM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_0,
				RG_VCM_PGA_LPM_SEL_MASK_SFT,
				0x9 << RG_VCM_PGA_LPM_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_0,
				RG_VCM_PGA_HIFI_SEL_MASK_SFT,
				0x8 << RG_VCM_PGA_HIFI_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUDLADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUDLADCFLASHFFCAPVREF_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCLFLASHVREFRES_LPM_MASK_SFT,
				0x1 << RG_AUDADCLFLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCLFLASHVREFRES_LPM2_MASK_SFT,
				0x0 << RG_AUDADCLFLASHVREFRES_LPM2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON28,
				RG_AUDRCTUNELSEL_MASK_SFT,
				0x0 << RG_AUDRCTUNELSEL_SFT);
			/* Write 5-bit audio L RC tune data */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON28,
				RG_AUDRCTUNEL_MASK_SFT,
				rc_tune << RG_AUDRCTUNEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON18,
				RG_AUDADCLCLKSEL_MASK_SFT,
				0x3 << RG_AUDADCLCLKSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDLADCHALFCLK_MASK_SFT,
				0x0 << RG_AUDLADCHALFCLK_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDLADCDACMODE_SEL_MASK_SFT,
				0x1 << RG_AUDLADCDACMODE_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON8,
				RG_AUDADC1STSTAGELPEN_MASK_SFT,
				0x1 << RG_AUDADC1STSTAGELPEN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON52,
				RG_AUDLADC1STSTAGELPEN_0_MASK_SFT,
				0x1 << RG_AUDLADC1STSTAGELPEN_0_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
				RG_AUDADCLMODE_MASK_SFT,
				0x3 << RG_AUDADCLMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON1,
				RG_AUDADCLPWRUP_MASK_SFT,
				0x1 << RG_AUDADCLPWRUP_SFT);
		} else {
			value = 0x1 << RG_AUDADCHIGHDR_EN_SFT |
					0x1 << RG_AUDADCHIGHDRSW_SEL_SFT |
					0x1 << RG_AUDADCHIGHDRSW_EN_SFT;
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT |
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT |
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				value << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON24,
				RG_AUDADCWIDECM_MASK_SFT,
				0x1 << RG_AUDADCWIDECM_SFT);
			/* Input resistor selection */
			if (priv->mic_hifi_mode)
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
					RG_AUDADCLRINOHM_MASK_SFT,
					0x1 << RG_AUDADCLRINOHM_SFT);
			else
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
					RG_AUDADCLRINOHM_MASK_SFT,
					0x4 << RG_AUDADCLRINOHM_SFT);
			regmap_write(priv->regmap, MT6338_AUDENC_ELR_0, 0x98);
			/* Selection of vref cap in flash */
			if ((priv->mic_hifi_mode == 0) && (priv->hw_ver == 3))
				regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
					RG_AUDLADCFLASHFFCAPVREF_SEL_MASK_SFT,
					0x3 << RG_AUDLADCFLASHFFCAPVREF_SEL_SFT);
			else
				regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
					RG_AUDLADCFLASHFFCAPVREF_SEL_MASK_SFT,
					0x1 << RG_AUDLADCFLASHFFCAPVREF_SEL_SFT);
			value = 0x0 << RG_AUDADCLFLASHVREFRES_LPM_SFT |
					0x0 << RG_AUDADCLFLASHVREFRES_LPM2_SFT;
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCLFLASHVREFRES_LPM_MASK_SFT |
				RG_AUDADCLFLASHVREFRES_LPM2_MASK_SFT,
				value << RG_AUDADCLFLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON28,
				RG_AUDRCTUNELSEL_MASK_SFT,
				0x0 << RG_AUDRCTUNELSEL_SFT);
			/* ADC clock selection */
			if (priv->mic_hifi_mode)
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON18,
					RG_AUDADCLCLKSEL_MASK_SFT,
					0x0 << RG_AUDADCLCLKSEL_SFT);
			else
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON18,
					RG_AUDADCLCLKSEL_MASK_SFT,
					0x3 << RG_AUDADCLCLKSEL_SFT);

			/* Half frquency enable */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDLADCHALFCLK_MASK_SFT,
				0x0 << RG_AUDLADCHALFCLK_SFT);
			if (priv->mic_hifi_mode) {
				/* ADC mode selection */
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
					RG_AUDLADCDACMODE_SEL_MASK_SFT,
					0x0 << RG_AUDLADCDACMODE_SEL_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON8,
					RG_AUDADC1STSTAGELPEN_MASK_SFT,
					0x0 << RG_AUDADC1STSTAGELPEN_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON52,
					RG_AUDLADC1STSTAGELPEN_0_MASK_SFT,
					0x0 << RG_AUDLADC1STSTAGELPEN_0_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
					RG_AUDADCLMODE_MASK_SFT,
					0x0 << RG_AUDADCLMODE_SFT);
			} else {
				/* ADC mode selection */
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
					RG_AUDLADCDACMODE_SEL_MASK_SFT,
					0x1 << RG_AUDLADCDACMODE_SEL_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON8,
					RG_AUDADC1STSTAGELPEN_MASK_SFT,
					0x1 << RG_AUDADC1STSTAGELPEN_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON52,
					RG_AUDLADC1STSTAGELPEN_0_MASK_SFT,
					0x1 << RG_AUDLADC1STSTAGELPEN_0_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
					RG_AUDADCLMODE_MASK_SFT,
					0x2 << RG_AUDADCLMODE_SFT);
			}
		}
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON1,
			RG_AUDADCLPWRUP_MASK_SFT,
			0x1 << RG_AUDADCLPWRUP_SFT);
		/* Audio L preamplifier DCC precharge */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON0,
			RG_AUDPREAMPLDCPRECHARGE_MASK_SFT,
			0x0 << RG_AUDPREAMPLDCPRECHARGE_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON1,
			RG_AUDADCLPWRUP_MASK_SFT,
			0x0 << RG_AUDADCLPWRUP_SFT);
		if ((priv->mic_hifi_mode == 0) && (priv->hw_ver == 3))
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUDLADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUDLADCFLASHFFCAPVREF_SEL_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_adc_r_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rc_tune = 0;
	unsigned int value = 0;

	dev_info(priv->dev, "%s(), event = 0x%x, vow_enable %d\n",
		 __func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
				RG_AUDADCRMODE_MASK_SFT,
				0x2 << RG_AUDADCRMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUDADCRWIDECM_MASK_SFT,
				0x1 << RG_AUDADCRWIDECM_SFT);
			/* ADC R input resistor value selection
			 * (10000)   8kohm (01000)  16kohm (00100)  32kohm
			 * (00010)  64kohm (00001) 128kohm (00000) 256kohm
			 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON46,
				RG_AUDADCRRINOHM_MASK_SFT,
				0x4 << RG_AUDADCRRINOHM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_2,
				RG_VCMR_PGA_LPM_SEL_MASK_SFT,
				0x9 << RG_VCMR_PGA_LPM_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_2,
				RG_VCMR_PGA_HIFI_SEL_MASK_SFT,
				0x8 << RG_VCMR_PGA_HIFI_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUDRADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUDRADCFLASHFFCAPVREF_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCRFLASHVREFRES_LPM_MASK_SFT,
				0x1 << RG_AUDADCRFLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCRFLASHVREFRES_LPM2_MASK_SFT,
				0x0 << RG_AUDADCRFLASHVREFRES_LPM2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON29,
				RG_AUDRCTUNER_MASK_SFT,
				0x4 << RG_AUDRCTUNER_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON29,
				RG_AUDRCTUNERSEL_MASK_SFT,
				0x0 << RG_AUDRCTUNERSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON19,
				RG_AUDADCRCLKSEL_MASK_SFT,
				0x3 << RG_AUDADCRCLKSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDRADCHALFCLK_MASK_SFT,
				0x0 << RG_AUDRADCHALFCLK_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDRADCDACMODE_SEL_MASK_SFT,
				0x1 << RG_AUDRADCDACMODE_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON16,
				RG_AUDRADC1STSTAGELPEN_MASK_SFT,
				0x1 << RG_AUDRADC1STSTAGELPEN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON40,
				RG_AUDRADC1STSTAGELPEN_0_MASK_SFT,
				0x1 << RG_AUDRADC1STSTAGELPEN_0_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
				RG_AUDADCRMODE_MASK_SFT,
				0x2 << RG_AUDADCRMODE_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (priv->vow_enable) {
			usleep_range(500, 520);
			/* Read 5-bit audio R RC tune data */
			regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON33, &rc_tune);
			dev_dbg(priv->dev, "%s(), vow rc_tune %d\n",
				 __func__, rc_tune);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON3,
				RG_AUDADCRPWRUP_MASK_SFT,
				0x0 << RG_AUDADCRPWRUP_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUDADCRWIDECM_MASK_SFT,
				0x1 << RG_AUDADCRWIDECM_SFT);
			/* ADC R input resistor value selection
			 * (10000)   8kohm (01000)  16kohm (00100)  32kohm
			 * (00010)  64kohm (00001) 128kohm (00000) 256kohm
			 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON46,
				RG_AUDADCRRINOHM_MASK_SFT,
				0x8 << RG_AUDADCRRINOHM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_2,
				RG_VCMR_PGA_LPM_SEL_MASK_SFT,
				0x9 << RG_VCMR_PGA_LPM_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_2,
				RG_VCMR_PGA_HIFI_SEL_MASK_SFT,
				0x8 << RG_VCMR_PGA_HIFI_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUDRADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUDRADCFLASHFFCAPVREF_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCRFLASHVREFRES_LPM_MASK_SFT,
				0x1 << RG_AUDADCRFLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCRFLASHVREFRES_LPM2_MASK_SFT,
				0x0 << RG_AUDADCRFLASHVREFRES_LPM2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON29,
				RG_AUDRCTUNERSEL_MASK_SFT,
				0x0 << RG_AUDRCTUNERSEL_SFT);
			/* Write 5-bit audio R RC tune data */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON29,
				RG_AUDRCTUNER_MASK_SFT,
				rc_tune << RG_AUDRCTUNER_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON19,
				RG_AUDADCRCLKSEL_MASK_SFT,
				0x3 << RG_AUDADCRCLKSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDRADCHALFCLK_MASK_SFT,
				0x0 << RG_AUDRADCHALFCLK_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDRADCDACMODE_SEL_MASK_SFT,
				0x1 << RG_AUDRADCDACMODE_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON16,
				RG_AUDRADC1STSTAGELPEN_MASK_SFT,
				0x1 << RG_AUDRADC1STSTAGELPEN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON40,
				RG_AUDRADC1STSTAGELPEN_0_MASK_SFT,
				0x1 << RG_AUDRADC1STSTAGELPEN_0_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
				RG_AUDADCRMODE_MASK_SFT,
				0x3 << RG_AUDADCRMODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON3,
				RG_AUDADCRPWRUP_MASK_SFT,
				0x1 << RG_AUDADCRPWRUP_SFT);
		} else {
			/* VICM loop control SW mode enable. */
			value = 0x1 << RG_AUDADCHIGHDR_EN_SFT |
					0x1 << RG_AUDADCHIGHDRSW_SEL_SFT |
					0x1 << RG_AUDADCHIGHDRSW_EN_SFT;
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT |
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT |
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				value << RG_AUDADCHIGHDR_EN_SFT);

			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUDADCRWIDECM_MASK_SFT,
				0x1 << RG_AUDADCRWIDECM_SFT);
			/* Input resistor selection */
			if (priv->mic_hifi_mode)
				regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON46, 0x1);
			else
				regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON46, 0x4);
			regmap_write(priv->regmap, MT6338_AUDENC_ELR_2, 0x98);
			/* Selection of vref cap in flash */
			if ((priv->mic_hifi_mode == 0) && (priv->hw_ver == 3))
				regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
					RG_AUDRADCFLASHFFCAPVREF_SEL_MASK_SFT,
					0x3 << RG_AUDRADCFLASHFFCAPVREF_SEL_SFT);
			else
				regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
					RG_AUDRADCFLASHFFCAPVREF_SEL_MASK_SFT,
					0x1 << RG_AUDRADCFLASHFFCAPVREF_SEL_SFT);
			value = 0x0 << RG_AUDADCRFLASHVREFRES_LPM_SFT |
					0x0 << RG_AUDADCRFLASHVREFRES_LPM2_SFT;
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADCRFLASHVREFRES_LPM_MASK_SFT |
				RG_AUDADCRFLASHVREFRES_LPM2_MASK_SFT,
				value << RG_AUDADCRFLASHVREFRES_LPM_SFT);

			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON29,
				RG_AUDRCTUNERSEL_MASK_SFT,
				0x0 << RG_AUDRCTUNERSEL_SFT);
			/* ADC clock selection */
			if (priv->mic_hifi_mode)
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON19,
					RG_AUDADCRCLKSEL_MASK_SFT,
					0x0 << RG_AUDADCRCLKSEL_SFT);
			else
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON19,
					RG_AUDADCRCLKSEL_MASK_SFT,
					0x3 << RG_AUDADCRCLKSEL_SFT);
			/* Half frquency enable */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUDRADCHALFCLK_MASK_SFT,
				0x0 << RG_AUDRADCHALFCLK_SFT);

			if (priv->mic_hifi_mode) {
				/* ADC mode selection */
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
					RG_AUDRADCDACMODE_SEL_MASK_SFT,
					0x0 << RG_AUDRADCDACMODE_SEL_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON16,
					RG_AUDRADC1STSTAGELPEN_MASK_SFT,
					0x0 << RG_AUDRADC1STSTAGELPEN_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON40,
					RG_AUDRADC1STSTAGELPEN_0_MASK_SFT,
					0x0 << RG_AUDRADC1STSTAGELPEN_0_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
					RG_AUDADCRMODE_MASK_SFT,
					0x0 << RG_AUDADCRMODE_SFT);
			} else {
				/* ADC mode selection */
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
					RG_AUDRADCDACMODE_SEL_MASK_SFT,
					0x1 << RG_AUDRADCDACMODE_SEL_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON16,
					RG_AUDRADC1STSTAGELPEN_MASK_SFT,
					0x1 << RG_AUDRADC1STSTAGELPEN_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON40,
					RG_AUDRADC1STSTAGELPEN_0_MASK_SFT,
					0x1 << RG_AUDRADC1STSTAGELPEN_0_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON14,
					RG_AUDADCRMODE_MASK_SFT,
					0x2 << RG_AUDADCRMODE_SFT);
			}
		}
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON3,
			RG_AUDADCRPWRUP_MASK_SFT,
			0x1 << RG_AUDADCRPWRUP_SFT);
		/* Audio R preamplifier DCC precharge */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON2,
			RG_AUDPREAMPRDCPRECHARGE_MASK_SFT,
			0x0 << RG_AUDPREAMPRDCPRECHARGE_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON3,
			RG_AUDADCRPWRUP_MASK_SFT,
			0x0 << RG_AUDADCRPWRUP_SFT);
		if ((priv->mic_hifi_mode == 0) && (priv->hw_ver == 3))
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUDRADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUDRADCFLASHFFCAPVREF_SEL_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_adc_3_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rc_tune = 0;
	unsigned int value = 0;

	dev_info(priv->dev, "%s(), event = 0x%x, vow_enable %d\n",
		 __func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
				RG_AUDADC3MODE_MASK_SFT,
				0x2 << RG_AUDADC3MODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUDADC3WIDECM_MASK_SFT,
				0x1 << RG_AUDADC3WIDECM_SFT);
			/* ADC 3 input resistor value selection
			 * (10000)   8kohm (01000)  16kohm (00100)  32kohm
			 * (00010)  64kohm (00001) 128kohm (00000) 256kohm
			 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON47,
				RG_AUDADC3RINOHM_MASK_SFT,
				0x4 << RG_AUDADC3RINOHM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_3,
				RG_VCM3_PGA_LPM_SEL_MASK_SFT,
				0x9 << RG_VCM3_PGA_LPM_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_3,
				RG_VCM3_PGA_HIFI_SEL_MASK_SFT,
				0x8 << RG_VCM3_PGA_HIFI_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUD3ADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUD3ADCFLASHFFCAPVREF_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC3FLASHVREFRES_LPM_MASK_SFT,
				0x1 << RG_AUDADC3FLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC3FLASHVREFRES_LPM2_MASK_SFT,
				0x0 << RG_AUDADC3FLASHVREFRES_LPM2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON30,
				RG_AUDRCTUNE3_MASK_SFT,
				0x4 << RG_AUDRCTUNE3_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON30,
				RG_AUDRCTUNE3SEL_MASK_SFT,
				0x0 << RG_AUDRCTUNE3SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON20,
				RG_AUDADC3CLKSEL_MASK_SFT,
				0x3 << RG_AUDADC3CLKSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUD3ADCHALFCLK_MASK_SFT,
				0x0 << RG_AUD3ADCHALFCLK_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
				RG_AUD3ADCDACMODE_SEL_MASK_SFT,
				0x1 << RG_AUD3ADCDACMODE_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON40,
				RG_AUD3ADC1STSTAGELPEN_MASK_SFT,
				0x3 << RG_AUD3ADC1STSTAGELPEN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
				RG_AUDADC3MODE_MASK_SFT,
				0x2 << RG_AUDADC3MODE_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (priv->vow_enable) {
			usleep_range(500, 520);
			/* Read 5-bit audio 3 RC tune data */
			regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON34, &rc_tune);
			dev_dbg(priv->dev, "%s(), vow rc_tune %d\n",
				 __func__, rc_tune);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON5,
				RG_AUDADC3PWRUP_MASK_SFT,
				0x0 << RG_AUDADC3PWRUP_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUDADC3WIDECM_MASK_SFT,
				0x1 << RG_AUDADC3WIDECM_SFT);
			/* ADC 3 input resistor value selection
			 * (10000)   8kohm (01000)  16kohm (00100)  32kohm
			 * (00010)  64kohm (00001) 128kohm (00000) 256kohm
			 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON47,
				RG_AUDADC3RINOHM_MASK_SFT,
				0x8 << RG_AUDADC3RINOHM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_3,
				RG_VCM3_PGA_LPM_SEL_MASK_SFT,
				0x9 << RG_VCM3_PGA_LPM_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_3,
				RG_VCM3_PGA_HIFI_SEL_MASK_SFT,
				0x8 << RG_VCM3_PGA_HIFI_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUD3ADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUD3ADCFLASHFFCAPVREF_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC3FLASHVREFRES_LPM_MASK_SFT,
				0x1 << RG_AUDADC3FLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC3FLASHVREFRES_LPM2_MASK_SFT,
				0x0 << RG_AUDADC3FLASHVREFRES_LPM2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON30,
				RG_AUDRCTUNE3SEL_MASK_SFT,
				0x0 << RG_AUDRCTUNE3SEL_SFT);
			/* Write 5-bit audio 3 RC tune data */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON30,
				RG_AUDRCTUNE3_MASK_SFT,
				rc_tune << RG_AUDRCTUNE3_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON20,
				RG_AUDADC3CLKSEL_MASK_SFT,
				0x3 << RG_AUDADC3CLKSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUD3ADCHALFCLK_MASK_SFT,
				0x0 << RG_AUD3ADCHALFCLK_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
				RG_AUD3ADCDACMODE_SEL_MASK_SFT,
				0x1 << RG_AUD3ADCDACMODE_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON40,
				RG_AUD3ADC1STSTAGELPEN_MASK_SFT,
				0x3 << RG_AUD3ADC1STSTAGELPEN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
				RG_AUDADC3MODE_MASK_SFT,
				0x3 << RG_AUDADC3MODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON5,
				RG_AUDADC3PWRUP_MASK_SFT,
				0x1 << RG_AUDADC3PWRUP_SFT);
		} else {
			/* VICM loop control SW mode enable. */
			value = 0x1 << RG_AUDADCHIGHDR_EN_SFT |
					0x1 << RG_AUDADCHIGHDRSW_SEL_SFT |
					0x1 << RG_AUDADCHIGHDRSW_EN_SFT;
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT |
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT |
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				value << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUDADC3WIDECM_MASK_SFT,
				0x1 << RG_AUDADC3WIDECM_SFT);
			/* Input resistor selection */
			if (priv->mic_hifi_mode)
				regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON47, 0x1);
			else
				regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON47, 0x4);

			regmap_write(priv->regmap, MT6338_AUDENC_ELR_3, 0x98);
			/* Selection of vref cap in flash */
			if ((priv->mic_hifi_mode == 0) && (priv->hw_ver == 3))
				regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
					RG_AUD3ADCFLASHFFCAPVREF_SEL_MASK_SFT,
					0x3 << RG_AUD3ADCFLASHFFCAPVREF_SEL_SFT);
			else
				regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
					RG_AUD3ADCFLASHFFCAPVREF_SEL_MASK_SFT,
					0x1 << RG_AUD3ADCFLASHFFCAPVREF_SEL_SFT);
			value = 0x0 << RG_AUDADC3FLASHVREFRES_LPM_SFT |
					0x0 << RG_AUDADC3FLASHVREFRES_LPM2_SFT;
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC3FLASHVREFRES_LPM_MASK_SFT |
				RG_AUDADC3FLASHVREFRES_LPM2_MASK_SFT,
				value << RG_AUDADC3FLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON30,
				RG_AUDRCTUNE3SEL_MASK_SFT,
				0x0 << RG_AUDRCTUNE3SEL_SFT);
			/* ADC clock selection */
			if (priv->mic_hifi_mode)
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON20,
					RG_AUDADC3CLKSEL_MASK_SFT,
					0x0 << RG_AUDADC3CLKSEL_SFT);
			else
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON20,
					RG_AUDADC3CLKSEL_MASK_SFT,
					0x3 << RG_AUDADC3CLKSEL_SFT);

			/* Half frquency enable */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUD3ADCHALFCLK_MASK_SFT,
				0x0 << RG_AUD3ADCHALFCLK_SFT);
			if (priv->mic_hifi_mode) {
				/* ADC mode selection */
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
					RG_AUD3ADCDACMODE_SEL_MASK_SFT,
					0x0 << RG_AUD3ADCDACMODE_SEL_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON40,
					RG_AUD3ADC1STSTAGELPEN_MASK_SFT,
					0x0 << RG_AUD3ADC1STSTAGELPEN_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
					RG_AUDADC3MODE_MASK_SFT,
					0x0 << RG_AUDADC3MODE_SFT);
			} else {
				/* ADC mode selection */
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
					RG_AUD3ADCDACMODE_SEL_MASK_SFT,
					0x1 << RG_AUD3ADCDACMODE_SEL_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON40,
					RG_AUD3ADC1STSTAGELPEN_MASK_SFT,
					0x3 << RG_AUD3ADC1STSTAGELPEN_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
					RG_AUDADC3MODE_MASK_SFT,
					0x2 << RG_AUDADC3MODE_SFT);
			}
		}
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON5,
			RG_AUDADC3PWRUP_MASK_SFT,
			0x1 << RG_AUDADC3PWRUP_SFT);
		/* Audio 3 preamplifier DCC precharge */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON4,
			RG_AUDPREAMP3DCPRECHARGE_MASK_SFT,
			0x0 << RG_AUDPREAMP3DCPRECHARGE_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON5,
			RG_AUDADC3PWRUP_MASK_SFT,
			0x0 << RG_AUDADC3PWRUP_SFT);
		if ((priv->mic_hifi_mode == 0) && (priv->hw_ver == 3))
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUD3ADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUD3ADCFLASHFFCAPVREF_SEL_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_adc_4_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rc_tune = 0;
	unsigned int value = 0;

	dev_info(priv->dev, "%s(), event = 0x%x, vow_enable %d\n",
		 __func__, event, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->vow_enable) {
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
				RG_AUDADC4MODE_MASK_SFT,
				0x2 << RG_AUDADC4MODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUDADC4WIDECM_MASK_SFT,
				0x1 << RG_AUDADC4WIDECM_SFT);
			/* ADC 4 input resistor value selection
			 * (10000)   8kohm (01000)  16kohm (00100)  32kohm
			 * (00010)  64kohm (00001) 128kohm (00000) 256kohm
			 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON48,
				RG_AUDADC4RINOHM_MASK_SFT,
				0x4 << RG_AUDADC4RINOHM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_4,
				RG_VCM4_PGA_LPM_SEL_MASK_SFT,
				0x9 << RG_VCM4_PGA_LPM_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_4,
				RG_VCM4_PGA_HIFI_SEL_MASK_SFT,
				0x8 << RG_VCM4_PGA_HIFI_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUD4ADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUD4ADCFLASHFFCAPVREF_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC4FLASHVREFRES_LPM_MASK_SFT,
				0x1 << RG_AUDADC4FLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC4FLASHVREFRES_LPM2_MASK_SFT,
				0x0 << RG_AUDADC4FLASHVREFRES_LPM2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON31,
				RG_AUDRCTUNE4_MASK_SFT,
				0x4 << RG_AUDRCTUNE4_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON31,
				RG_AUDRCTUNE4SEL_MASK_SFT,
				0x0 << RG_AUDRCTUNE4SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON21,
				RG_AUDADC4CLKSEL_MASK_SFT,
				0x3 << RG_AUDADC4CLKSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUD4ADCHALFCLK_MASK_SFT,
				0x0 << RG_AUD4ADCHALFCLK_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
				RG_AUD4ADCDACMODE_SEL_MASK_SFT,
				0x1 << RG_AUD4ADCDACMODE_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUD4ADC1STSTAGELPEN_MASK_SFT,
				0x3 << RG_AUD4ADC1STSTAGELPEN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
				RG_AUDADC4MODE_MASK_SFT,
				0x2 << RG_AUDADC4MODE_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (priv->vow_enable) {
			usleep_range(500, 520);
			/* Read 5-bit audio 4 RC tune data */
			regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON35, &rc_tune);
			dev_dbg(priv->dev, "%s(), vow rc_tune %d\n",
				 __func__, rc_tune);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON7,
				RG_AUDADC4PWRUP_MASK_SFT,
				0x0 << RG_AUDADC4PWRUP_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				0x1 << RG_AUDADCHIGHDRSW_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUDADC4WIDECM_MASK_SFT,
				0x1 << RG_AUDADC4WIDECM_SFT);
			/* ADC 4 input resistor value selection
			 * (10000)   8kohm (01000)  16kohm (00100)  32kohm
			 * (00010)  64kohm (00001) 128kohm (00000) 256kohm
			 */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON48,
				RG_AUDADC4RINOHM_MASK_SFT,
				0x8 << RG_AUDADC4RINOHM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_4,
				RG_VCM4_PGA_LPM_SEL_MASK_SFT,
				0x9 << RG_VCM4_PGA_LPM_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_4,
				RG_VCM4_PGA_HIFI_SEL_MASK_SFT,
				0x8 << RG_VCM4_PGA_HIFI_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUD4ADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUD4ADCFLASHFFCAPVREF_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC4FLASHVREFRES_LPM_MASK_SFT,
				0x1 << RG_AUDADC4FLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC4FLASHVREFRES_LPM2_MASK_SFT,
				0x0 << RG_AUDADC4FLASHVREFRES_LPM2_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON31,
				RG_AUDRCTUNE4SEL_MASK_SFT,
				0x0 << RG_AUDRCTUNE4SEL_SFT);
			/* Write 5-bit audio 4 RC tune data */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON31,
				RG_AUDRCTUNE4_MASK_SFT,
				rc_tune << RG_AUDRCTUNE4_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON21,
				RG_AUDADC4CLKSEL_MASK_SFT,
				0x3 << RG_AUDADC4CLKSEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUD4ADCHALFCLK_MASK_SFT,
				0x0 << RG_AUD4ADCHALFCLK_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
				RG_AUD4ADCDACMODE_SEL_MASK_SFT,
				0x1 << RG_AUD4ADCDACMODE_SEL_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUD4ADC1STSTAGELPEN_MASK_SFT,
				0x3 << RG_AUD4ADC1STSTAGELPEN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
				RG_AUDADC4MODE_MASK_SFT,
				0x3 << RG_AUDADC4MODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON7,
				RG_AUDADC4PWRUP_MASK_SFT,
				0x1 << RG_AUDADC4PWRUP_SFT);
		} else {
			/* VICM loop control SW mode enable. */
			value = 0x1 << RG_AUDADCHIGHDR_EN_SFT |
					0x1 << RG_AUDADCHIGHDRSW_SEL_SFT |
					0x1 << RG_AUDADCHIGHDRSW_EN_SFT;
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON49,
				RG_AUDADCHIGHDR_EN_MASK_SFT |
				RG_AUDADCHIGHDRSW_SEL_MASK_SFT |
				RG_AUDADCHIGHDRSW_EN_MASK_SFT,
				value << RG_AUDADCHIGHDR_EN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
				RG_AUDADC4WIDECM_MASK_SFT,
				0x1 << RG_AUDADC4WIDECM_SFT);
			/* Input resistor selection */

			if (priv->mic_hifi_mode)
				regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON48, 0x1);
			else
				regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON48, 0x4);
			regmap_write(priv->regmap, MT6338_AUDENC_ELR_4, 0x98);
			/* Selection of vref cap in flash */
			if ((priv->mic_hifi_mode == 0) && (priv->hw_ver == 3))
				regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
					RG_AUD4ADCFLASHFFCAPVREF_SEL_MASK_SFT,
					0x3 << RG_AUD4ADCFLASHFFCAPVREF_SEL_SFT);
			else
				regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
					RG_AUD4ADCFLASHFFCAPVREF_SEL_MASK_SFT,
					0x1 << RG_AUD4ADCFLASHFFCAPVREF_SEL_SFT);
			value = 0x0 << RG_AUDADC4FLASHVREFRES_LPM_SFT |
					0x0 << RG_AUDADC4FLASHVREFRES_LPM2_SFT;
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON50,
				RG_AUDADC4FLASHVREFRES_LPM_MASK_SFT |
				RG_AUDADC4FLASHVREFRES_LPM2_MASK_SFT,
				value << RG_AUDADC4FLASHVREFRES_LPM_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON31,
				RG_AUDRCTUNE4SEL_MASK_SFT,
				0x0 << RG_AUDRCTUNE4SEL_SFT);
			/* ADC clock selection */
			if (priv->mic_hifi_mode)
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON21,
					RG_AUDADC4CLKSEL_MASK_SFT,
					0x0 << RG_AUDADC4CLKSEL_SFT);
			else
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON21,
					RG_AUDADC4CLKSEL_MASK_SFT,
					0x3 << RG_AUDADC4CLKSEL_SFT);
			/* Half frquency enable */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON44,
				RG_AUD4ADCHALFCLK_MASK_SFT,
				0x0 << RG_AUD4ADCHALFCLK_SFT);
			if (priv->mic_hifi_mode) {
				/* ADC mode selection */
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
					RG_AUD4ADCDACMODE_SEL_MASK_SFT,
					0x0 << RG_AUD4ADCDACMODE_SEL_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
					RG_AUD4ADC1STSTAGELPEN_MASK_SFT,
					0x0 << RG_AUD4ADC1STSTAGELPEN_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
					RG_AUDADC4MODE_MASK_SFT,
					0x0 << RG_AUDADC4MODE_SFT);
			} else {
				/* ADC mode selection */
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON45,
					RG_AUD4ADCDACMODE_SEL_MASK_SFT,
					0x1 << RG_AUD4ADCDACMODE_SEL_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON41,
					RG_AUD4ADC1STSTAGELPEN_MASK_SFT,
					0x3 << RG_AUD4ADC1STSTAGELPEN_SFT);
				regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON15,
					RG_AUDADC4MODE_MASK_SFT,
					0x2 << RG_AUDADC4MODE_SFT);
			}
		}
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON7,
			RG_AUDADC4PWRUP_MASK_SFT,
			0x1 << RG_AUDADC4PWRUP_SFT);
		/* Audio 4 preamplifier DCC precharge */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON6,
			RG_AUDPREAMP4DCPRECHARGE_MASK_SFT,
			0x0 << RG_AUDPREAMP4DCPRECHARGE_SFT);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON7,
			RG_AUDADC4PWRUP_MASK_SFT,
			0x0 << RG_AUDADC4PWRUP_SFT);
		if ((priv->mic_hifi_mode == 0) && (priv->hw_ver == 3))
			regmap_update_bits(priv->regmap, MT6338_AUDENC_ELR_1,
				RG_AUD4ADCFLASHFFCAPVREF_SEL_MASK_SFT,
				0x1 << RG_AUD4ADCFLASHFFCAPVREF_SEL_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_pga_l_mux_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);

	dev_dbg(priv->dev, "%s(), mux %d\n", __func__, mux);
	priv->mux_select[MUX_PGA_L] = mux >> RG_AUDPREAMPLINPUTSEL_SFT;
	return 0;
}

static int mt_pga_r_mux_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);

	dev_dbg(priv->dev, "%s(), mux %d\n", __func__, mux);
	priv->mux_select[MUX_PGA_R] = mux >> RG_AUDPREAMPRINPUTSEL_SFT;
	return 0;
}

static int mt_pga_3_mux_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);

	dev_dbg(priv->dev, "%s(), mux %d\n", __func__, mux);
	priv->mux_select[MUX_PGA_3] = mux >> RG_AUDPREAMP3INPUTSEL_SFT;
	return 0;
}

static int mt_pga_4_mux_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux = dapm_kcontrol_get_value(w->kcontrols[0]);

	dev_dbg(priv->dev, "%s(), mux %d\n", __func__, mux);
	priv->mux_select[MUX_PGA_4] = mux >> RG_AUDPREAMP4INPUTSEL_SFT;
	return 0;
}

static int mt_pga_l_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux_pga = priv->mux_select[MUX_PGA_L];
	unsigned int mic_type;
	int mic_gain_l;

	switch (mux_pga) {
	case PGA_MUX_AIN0:
		mic_type = priv->mux_select[MUX_MIC_TYPE_0];
		break;
	case PGA_MUX_AIN1:
		mic_type = priv->mux_select[MUX_MIC_TYPE_1];
		break;
	case PGA_MUX_AIN2:
		mic_type = priv->mux_select[MUX_MIC_TYPE_2];
		break;
	default:
		dev_err(priv->dev, "%s(), invalid pga mux %d\n",
			__func__, mux_pga);
		return -EINVAL;
	}

	/* if vow is enabled, always set volume as 6 (18dB) */
	mic_gain_l = priv->vow_enable ? 6 :
		     priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP1];
	dev_info(priv->dev, "%s(), event = 0x%x, mic_type %d, mic_gain_l %d, mux_pga %d, vow_enable %d\n",
		__func__, event, mic_type, mic_gain_l, mux_pga, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (IS_DCC_BASE(mic_type)) {
			/* Audio L preamplifier DCC precharge */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON0,
				RG_AUDPREAMPLDCPRECHARGE_MASK_SFT,
				0x1 << RG_AUDPREAMPLDCPRECHARGE_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* set mic pga gain */
		/* (0010)  0dB (0011)  3dB (0100)  6dB (0101)  9dB (0110) 12dB
		 * (0111) 15dB (1000) 18dB (1001) 21dB (1010) 24dB
		 */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON1,
			RG_AUDPREAMPLGAIN_MASK_SFT,
			mic_gain_l << RG_AUDPREAMPLGAIN_SFT);

		if (IS_DCC_BASE(mic_type)) {
			/* L preamplifier DCCEN */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON0,
				RG_AUDPREAMPLDCCEN_MASK_SFT,
				0x1 << RG_AUDPREAMPLDCCEN_SFT);
		} else {
			/* Audio L preamplifier ACC gain adjust */
			/* (000) 0dB, (001) 6dB, (010) 12dB (011) 18dB, (100) 24dB */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON38,
			   RG_AUDPREAMPLACCGAIN_MASK_SFT,
			   0x0 << RG_AUDPREAMPLACCGAIN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON0,
			   RG_AUDPREAMPLDCCEN_MASK_SFT,
			   0x0 << RG_AUDPREAMPLDCCEN_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* L preamplifier DCCEN */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON0,
			RG_AUDPREAMPLDCCEN_MASK_SFT,
			0x0 << RG_AUDPREAMPLDCCEN_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_pga_r_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux_pga = priv->mux_select[MUX_PGA_R];
	unsigned int mic_type;
	int mic_gain_r;

	switch (mux_pga) {
	case PGA_MUX_AIN0:
		mic_type = priv->mux_select[MUX_MIC_TYPE_0];
		break;
	case PGA_MUX_AIN1:
		mic_type = priv->mux_select[MUX_MIC_TYPE_1];
		break;
	case PGA_MUX_AIN2:
		mic_type = priv->mux_select[MUX_MIC_TYPE_2];
		break;
	default:
		dev_err(priv->dev, "%s(), invalid pga mux %d\n",
			__func__, mux_pga);
		return -EINVAL;
	}

	/* if vow is enabled, always set volume as 6 (18dB) */
	mic_gain_r = priv->vow_enable ? 6 :
		     priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP2];
	dev_info(priv->dev, "%s(), event = 0x%x, mic_type %d, mic_gain_r %d, mux_pga %d, vow_enable %d\n",
		__func__, event, mic_type, mic_gain_r, mux_pga, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (IS_DCC_BASE(mic_type)) {
			/* Audio R preamplifier DCC precharge */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON2,
				RG_AUDPREAMPRDCPRECHARGE_MASK_SFT,
				0x1 << RG_AUDPREAMPRDCPRECHARGE_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* set mic pga gain */
		/* (0010)  0dB (0011)  3dB (0100)  6dB (0101)  9dB (0110) 12dB
		 * (0111) 15dB (1000) 18dB (1001) 21dB (1010) 24dB
		 */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON3,
			RG_AUDPREAMPRGAIN_MASK_SFT,
			mic_gain_r << RG_AUDPREAMPRGAIN_SFT);

		if (IS_DCC_BASE(mic_type)) {
			/* R preamplifier DCCEN */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON2,
				RG_AUDPREAMPRDCCEN_MASK_SFT,
				0x1 << RG_AUDPREAMPRDCCEN_SFT);
		} else {
			/* Audio R preamplifier ACC gain adjust */
			/* (000) 0dB, (001) 6dB, (010) 12dB (011) 18dB, (100) 24dB */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON38,
			   RG_AUDPREAMPRACCGAIN_MASK_SFT,
			   0x0 << RG_AUDPREAMPRACCGAIN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON2,
			   RG_AUDPREAMPRDCCEN_MASK_SFT,
			   0x0 << RG_AUDPREAMPRDCCEN_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* R preamplifier DCCEN */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON2,
			RG_AUDPREAMPRDCCEN_MASK_SFT,
			0x0 << RG_AUDPREAMPRDCCEN_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_pga_3_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int mux_pga = priv->mux_select[MUX_PGA_3];
	unsigned int mic_type;
	int mic_gain_3;

	switch (mux_pga) {
	case PGA_3_MUX_AIN0:
		mic_type = priv->mux_select[MUX_MIC_TYPE_0];
		break;
	case PGA_3_MUX_AIN2:
		mic_type = priv->mux_select[MUX_MIC_TYPE_2];
		break;
	case PGA_3_MUX_AIN3:
	case PGA_3_MUX_AIN5:
		mic_type = priv->mux_select[MUX_MIC_TYPE_3];
		break;
	default:
		dev_err(priv->dev, "%s(), invalid pga mux %d\n",
			__func__, mux_pga);
		return -EINVAL;
	}

	/* if vow is enabled, always set volume as 10 (24dB) */
	mic_gain_3 = priv->vow_enable ? 10 :
		     priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP3];
	dev_info(priv->dev, "%s(), event = 0x%x, mic_type %d, mic_gain_3 %d, mux_pga %d, vow_enable %d\n",
		__func__, event, mic_type, mic_gain_3, mux_pga, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (IS_DCC_BASE(mic_type)) {
			/* Audio 3 preamplifier DCC precharge */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON4,
				RG_AUDPREAMP3DCPRECHARGE_MASK_SFT,
				0x1 << RG_AUDPREAMP3DCPRECHARGE_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* set mic pga gain */
		/* (0010)  0dB (0011)  3dB (0100)  6dB (0101)  9dB (0110) 12dB
		 * (0111) 15dB (1000) 18dB (1001) 21dB (1010) 24dB
		 */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON5,
			RG_AUDPREAMP3GAIN_MASK_SFT,
			mic_gain_3 << RG_AUDPREAMP3GAIN_SFT);

		if (IS_DCC_BASE(mic_type)) {
			/* 3 preamplifier DCCEN */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON4,
				RG_AUDPREAMP3DCCEN_MASK_SFT,
				0x1 << RG_AUDPREAMP3DCCEN_SFT);
		} else {
			/* Audio 3 preamplifier ACC gain adjust */
			/* (000) 0dB, (001) 6dB, (010) 12dB (011) 18dB, (100) 24dB */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON39,
				RG_AUDPREAMP3ACCGAIN_MASK_SFT,
				0x0 << RG_AUDPREAMP3ACCGAIN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON4,
				RG_AUDPREAMP3DCCEN_MASK_SFT,
				0x0 << RG_AUDPREAMP3DCCEN_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 3 preamplifier DCCEN */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON4,
			RG_AUDPREAMP3DCCEN_MASK_SFT,
			0x0 << RG_AUDPREAMP3DCCEN_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_pga_4_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int mic_gain_4 = priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP4];
	unsigned int mux_pga = priv->mux_select[MUX_PGA_4];
	unsigned int mic_type;

	switch (mux_pga) {
	case PGA_4_MUX_AIN2:
		mic_type = priv->mux_select[MUX_MIC_TYPE_2];
		break;
	case PGA_4_MUX_AIN3:
	case PGA_4_MUX_AIN4:
	case PGA_4_MUX_AIN6:
		mic_type = priv->mux_select[MUX_MIC_TYPE_3];
		break;
	default:
		dev_err(priv->dev, "%s(), invalid pga mux %d\n",
			__func__, mux_pga);
		return -EINVAL;
	}

	/* if vow is enabled, always set volume as 10 (24dB) */
	mic_gain_4 = priv->vow_enable ? 10 :
		     priv->ana_gain[AUDIO_ANALOG_VOLUME_MICAMP4];
	dev_info(priv->dev, "%s(), event = 0x%x, mic_type %d, mic_gain_4 %d, mux_pga %d, vow_enable %d\n",
		__func__, event, mic_type, mic_gain_4, mux_pga, priv->vow_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (IS_DCC_BASE(mic_type)) {
			/* Audio 4 preamplifier DCC precharge */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON6,
				RG_AUDPREAMP4DCPRECHARGE_MASK_SFT,
				0x1 << RG_AUDPREAMP4DCPRECHARGE_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* set mic pga gain */
		/* (0010)  0dB (0011)  3dB (0100)  6dB (0101)  9dB (0110) 12dB
		 * (0111) 15dB (1000) 18dB (1001) 21dB (1010) 24dB
		 */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON7,
			RG_AUDPREAMP4GAIN_MASK_SFT,
			mic_gain_4 << RG_AUDPREAMP4GAIN_SFT);

		if (IS_DCC_BASE(mic_type)) {
			/* 4 preamplifier DCCEN */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON6,
				RG_AUDPREAMP4DCCEN_MASK_SFT,
				0x1 << RG_AUDPREAMP4DCCEN_SFT);
		} else {
			/* Audio 4 preamplifier ACC gain adjust */
			/* (000) 0dB, (001) 6dB, (010) 12dB (011) 18dB, (100) 24dB */
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON39,
				RG_AUDPREAMP4ACCGAIN_MASK_SFT,
				0x0 << RG_AUDPREAMP4ACCGAIN_SFT);
			regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON6,
				RG_AUDPREAMP4DCCEN_MASK_SFT,
				0x0 << RG_AUDPREAMP4DCCEN_SFT);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 4 preamplifier DCCEN */
		regmap_update_bits(priv->regmap, MT6338_AUDENC_PMU_CON6,
			RG_AUDPREAMP4DCCEN_MASK_SFT,
			0x0 << RG_AUDPREAMP4DCCEN_SFT);
		break;
	default:
		break;
	}

	return 0;
}

/* It is based on hw's control sequenece to add some delay when PMU/PMD */
static int mt_delay_100_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
	case SND_SOC_DAPM_PRE_PMD:
		usleep_range(100, 120);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_hp_pull_down_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol,
				 int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		hp_pull_down(priv, true, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		hp_pull_down(priv, true, false);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_hp_mute_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Set HPR/HPL gain to -10dB */
		regmap_write(priv->regmap, MT6338_ZCD_CON2, HP_GAIN_0DB);
		regmap_write(priv->regmap, MT6338_ZCD_CON2_H, HP_GAIN_0DB);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Set HPL/HPR gain to mute */
		regmap_write(priv->regmap, MT6338_ZCD_CON2, HP_GAIN_0DB);
		regmap_write(priv->regmap, MT6338_ZCD_CON2_H, HP_GAIN_0DB);

		break;
	default:
		break;
	}

	return 0;
}

static int mt_hp_damp_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_POST_PMD:
		/* Disable HP damping circuit & HPN 4K load */
		/* reset CMFB PW level */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
			RG_AUDHPDAMP_ADJ_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHPDAMP_ADJ_VAUDP18_SFT);
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
			RG_AUDHPDAMP_EN_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHPDAMP_EN_VAUDP18_SFT);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_hp_ana_trim_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	struct hp_trim_data *trim;
	unsigned int value = 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* TODO: 3/4 pole */
		trim = &priv->hp_trim_3_pole;

		/* set hp l trim */
		value = trim->hp_trim_l << RG_AUDHPLTRIM_VAUDP18_SFT |
				trim->hp_fine_trim_l << RG_AUDHPLFINETRIM_VAUDP18_SFT;
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON12, value);

		/* set hp r trim */
		value = trim->hp_trim_r << RG_AUDHPLTRIM_VAUDP18_SFT |
				trim->hp_fine_trim_r << RG_AUDHPLFINETRIM_VAUDP18_SFT;
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON13, value);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Clear the analog trim value */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON12, 0x0);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON13, 0x0);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_esd_resist_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Reduce ESD resistance of AU_REFN */
		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON10,
			RG_AUDREFN_DERES_EN_VAUDP18_MASK_SFT,
			0x1 << RG_AUDREFN_DERES_EN_VAUDP18_SFT);
		usleep_range(250, 270);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		break;
	}

	return 0;
}

static int mt_ldo_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_dbg(priv->dev, "%s()\n", __func__);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enable for V32REFGEN */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x7);
		usleep_range(100, 120);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Disnable for V32REFGEN */
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON37, 0x0);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x0);
		regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON39, 0x0);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_sdm_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol,
			int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rate = 0;

	dev_dbg(priv->dev, "%s() dl sample_rate = %d", __func__, priv->dl_rate[0]);
	if (priv->dl_rate[0] != 0)
		rate = mt6338_rate_transform(priv->dl_rate[0]);
	else
		rate = MT6338_ADDA_48000HZ;
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* select 12bit 2nd SDM  */
		regmap_write(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_H, 0x80);
		/* To do :64tap dither */
		regmap_write(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DITHER_CON_M, 0x11);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0_H,
			AFE_DL_INPUT_MODE_CTL_MASK_SFT,
			0x0 << AFE_DL_INPUT_MODE_CTL_SFT);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DITHER_CON_M, 0x0);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_sdm_3rd_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rate;

	dev_dbg(priv->dev, "%s() dl sample_rate = %d", __func__, priv->dl_rate[0]);
	if (priv->dl_rate[0] != 0)
		rate = mt6338_rate_transform(priv->dl_rate[0]);
	else
		rate = MT6338_ADDA_48000HZ;
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		 /* select 12bit 2nd SDM  */
		regmap_write(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON_H, 0x0);
		/* dither */
		regmap_write(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_DITHER_CON_M, 0x0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0_H,
			AFE_2ND_DL_INPUT_MODE_CTL_MASK_SFT,
			0x0 << AFE_2ND_DL_INPUT_MODE_CTL_SFT);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_DITHER_CON_M, 0x0);
		break;
	default:
		break;
	}

	return 0;
}
static int mt_top_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->hp_hifi_mode == 2) {
			regmap_write(priv->regmap, MT6338_AFE_GAIN1_CON1_2, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_GAIN1_CON1_1, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_GAIN1_CON1_0, 0x1);
			regmap_write(priv->regmap, MT6338_AFE_GAIN1_CON0_0, 0xa0);
		}
		break;
	default:
		break;
	}
	return 0;
}

static int mt_nle_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->hp_hifi_mode == 2) {
			/* LCH gain */
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_H,
				RG_DG_STEP_LCH_SW_CONFIG_MODE_MASK_SFT,
				0x1 << RG_DG_STEP_LCH_SW_CONFIG_MODE_SFT);
			/* LCH gain G0: 0x010000 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_M, 0x1);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_L, 0x0);

			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0, 0x0);
			/* LCH gain G1: 0x016900 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_M, 0x1);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_L, 0xb);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1, 0x5d);
			/* LCH gain G2: 0x01fe00 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_M, 0x1);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_L, 0x7a);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2, 0xf2);
			/* LCH gain G3: 0x02d100 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_M, 0x2);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_L, 0x19);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3, 0xa8);
			/* LCH gain G4: 0x03fb00 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_M, 0x2);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_L, 0xe6);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4, 0x96);
			/* LCH gain G5: 0x059f00 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_M, 0x4);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_L, 0x57);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5, 0xb3);
			/* LCH gain G6: 0x07f100 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_M, 0x6);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_L, 0x49);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6, 0x06);
			/* LCH gain G7: 0x0b3800 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_M, 0x8);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_L, 0xfb);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7, 0x32);

			/* RCH gain */
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_H,
				RG_DG_STEP_RCH_SW_CONFIG_MODE_MASK_SFT,
				0x1 << RG_DG_STEP_RCH_SW_CONFIG_MODE_SFT);
			/* RCH gain G0: 0x010000 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_M, 0x1);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_L, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0, 0x0);
			/* RCH gain G1: 0x016900 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_M, 0x1);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_L, 0xc);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1, 0xcc);
			/* RCH gain G2: 0x01fe00 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_M, 0x1);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_L, 0x7b);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2, 0x4e);
			/* RCH gain G3: 0x02d100 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_M, 0x2);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_L, 0x19);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3, 0x3b);
			/* RCH gain G4: 0x03fb00 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_M, 0x2);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_L, 0xe5);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4, 0x98);
			/* RCH gain G5: 0x059f00 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_M, 0x4);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_L, 0x53);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5, 0x4f);
			/* RCH gain G6: 0x07f100 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_M, 0x6);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_L, 0x3f);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6, 0x93);
			/* RCH gain G7: 0x0b3800 */
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_H, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_M, 0x8);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_L, 0xcc);
			regmap_write(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7, 0xa9);

			/* preview window */
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_PRE_BUF_CFG_H,
				BYPASS_DELAY_MASK_SFT, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_PRE_BUF_CFG_M, 0x3b);
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_PRE_BUF_CFG_L,
				POINT_END_H_MASK_SFT, 0x3);
			regmap_write(priv->regmap, MT6338_AFE_NLE_PRE_BUF_CFG, 0xb0);
			/* NLE hold time 22ms must larger than preview window */
			regmap_write(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_CFG_H, 0x1c);
			regmap_write(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_CFG_H, 0x1c);

			/* power detect -45dB */
			regmap_write(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_CFG_M, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_CFG_L, 0xb8);
			regmap_write(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_CFG, 0x45);
			regmap_write(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_CFG_M, 0x0);
			regmap_write(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_CFG_L, 0xb8);
			regmap_write(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_CFG, 0x45);

			/* ZCD detect HW check */
			regmap_write(priv->regmap, MT6338_AFE_NLE_ZCD_LCH_CFG, 0x2);
			regmap_write(priv->regmap, MT6338_AFE_NLE_ZCD_RCH_CFG, 0x2);
			/* NLE gain setting time out 22ms */
			regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_H, 0x16);
			regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_H, 0x16);
			/* Set AG gain min & max */
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0,
				RG_AG_MAX_LCH_MASK_SFT,
				0x0 << RG_AG_MAX_LCH_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0,
				RG_AG_MIN_LCH_MASK_SFT,
				0x7 << RG_AG_MIN_LCH_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0,
				RG_AG_MAX_RCH_MASK_SFT,
				0x0 << RG_AG_MAX_RCH_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0,
				RG_AG_MIN_RCH_MASK_SFT,
				0x7 << RG_AG_MIN_RCH_SFT);

			/* AG delay time to analog domain 19T */
			regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_M, 0x13);
			regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_M, 0x13);

			/* Set Dgain & Again in debug mode */
			regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_L, 0x7);
			regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0, 0x0);

			regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_L, 0x7);
			regmap_write(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0, 0x0);

			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_M,
				RG_GAIN_STEP_PER_JUMP_LCH_MASK_SFT,
				0x0 << RG_GAIN_STEP_PER_JUMP_LCH_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_M,
				RG_HOLD_TIME_PER_JUMP_LCH_MASK_SFT,
				0x0 << RG_HOLD_TIME_PER_JUMP_LCH_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_L,
				RG_GAIN_STEP_PER_ZCD_LCH_MASK_SFT,
				0x0 << RG_GAIN_STEP_PER_ZCD_LCH_SFT);

			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_M,
				RG_GAIN_STEP_PER_JUMP_RCH_MASK_SFT,
				0x0 << RG_GAIN_STEP_PER_JUMP_RCH_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_M,
				RG_HOLD_TIME_PER_JUMP_RCH_MASK_SFT,
				0x0 << RG_HOLD_TIME_PER_JUMP_RCH_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_L,
				RG_GAIN_STEP_PER_ZCD_RCH_MASK_SFT,
				0x0 << RG_GAIN_STEP_PER_ZCD_RCH_SFT);

			/* NLE ON */
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_CFG,
				RG_LOW_LATENCY_MODE_MASK_SFT,
				0x0 << RG_LOW_LATENCY_MODE_SFT);
			regmap_update_bits(priv->regmap, MT6338_AFE_NLE_CFG,
				RG_BYPASS_NLE_MASK_SFT,
				0x0 << RG_BYPASS_NLE_SFT);
		}
		break;
	default:
		break;
	}

	return 0;
}
static int mt_dl_gpio_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6338_set_playback_gpio(priv);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6338_reset_playback_gpio(priv);
		break;
	default:
		break;
	}
	return 0;
}

static int mt_ul_gpio_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6338_set_capture_gpio(priv);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6338_reset_capture_gpio(priv);
		break;
	default:
		break;
	}
	return 0;
}

static int mt_dl_src_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6338_set_dl_src(priv, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6338_set_dl_src(priv, false);
		break;
	default:
		break;
	}
	return 0;
}
static int mt_2nd_dl_src_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6338_set_2nd_dl_src(priv, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mt6338_set_2nd_dl_src(priv, false);
		break;
	default:
		break;
	}
	return 0;
}

static int dc_trim_thread(void *arg);
static int mt_dc_trim_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	struct dc_trim_data *dc_trim = &priv->dc_trim;

	dev_dbg(priv->dev, "%s(), event = 0x%x, dc_trim->calibrated %u\n",
		 __func__, event, dc_trim->calibrated);

	if (dc_trim->calibrated)
		return 0;

	kthread_run(dc_trim_thread, priv, "dc_trim_thread");
	return 0;
}

/* DAPM Widgets */
static const struct snd_soc_dapm_widget mt6338_dapm_widgets[] = {
	/* Global Supply*/
	SND_SOC_DAPM_SUPPLY_S("KEY", SUPPLY_SEQ_CLK_BUF,
			      SND_SOC_NOPM, 0, 0,
				  mt_key_event,
				  SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("CLK_BUF", SUPPLY_SEQ_CLK_BUF,
			      SND_SOC_NOPM, 0, 0,
				  mt_dcxo_event,
				  SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("NCP_CK", SUPPLY_SEQ_CKTST,
				  MT6338_TOP_CKTST_CON0,
				  RG_VCORE_26M_CK_TST_DIS_SFT, 0, NULL, 0),
	/* set when 208M*/
	SND_SOC_DAPM_SUPPLY_S("NCP_CK_208", SUPPLY_SEQ_CKTST,
				  MT6338_TOP_CKTST_CON0,
				  RG_DSPPLL_208M_CK_TST_DIS_SFT, 0, NULL, 0),
	/* SND_SOC_DAPM_REGULATOR_SUPPLY("vaud18", 0, 0),*/
	SND_SOC_DAPM_SUPPLY_S("LDO_VAUD18", SUPPLY_SEQ_LDO_VAUD18,
			      SND_SOC_NOPM, 0, 0,
			      mt_vaud18_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("AUDGLB", SUPPLY_SEQ_AUD_GLB,
				  MT6338_AUDDEC_PMU_CON34,
			      RG_AUDGLB_PWRDN_VA32_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("PLL18 Audio", SUPPLY_SEQ_PLL_208M,
			      SND_SOC_NOPM, 0, 0,
			      mt_pll208m_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("PLL18 EN", SUPPLY_SEQ_PLL_208M,
				  SND_SOC_NOPM, 0, 0,
				  mt_vpll18_event,
				  SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("CLKSQ Audio", SUPPLY_SEQ_CLKSQ,
			      SND_SOC_NOPM, 0, 0,
			      mt_clksq_event,
			      SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY_S("AUDNCP_CK", SUPPLY_SEQ_TOP_CK,
			      MT6338_AUD_TOP_CKPDN_CON0,
			      RG_AUDNCP_CK_PDN_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ZCD13M_CK", SUPPLY_SEQ_TOP_CK,
			      MT6338_AUD_TOP_CKPDN_CON0,
			      RG_ZCD13M_CK_PDN_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUD_CK", SUPPLY_SEQ_TOP_CK_LAST,
			      MT6338_AUD_TOP_CKPDN_CON0,
			      RG_AUD_CK_PDN_SFT, 1,
			      mt_delay_100_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY_S("AUDIF_CK", SUPPLY_SEQ_TOP_CK,
			      MT6338_AUD_TOP_CKPDN_CON0,
			      RG_AUDIF_CK_PDN_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUD208M", SUPPLY_SEQ_TOP_CK,
				  SND_SOC_NOPM, 0, 0,
				  mt_aud208_event,
				  SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("SRAM Audio", SUPPLY_SEQ_TOP_SRAM,
				  SND_SOC_NOPM, 0, 0,
				  mt_sram_event,
				  SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
	SND_SOC_DAPM_SUPPLY_S("PLL18_VOW", SUPPLY_SEQ_PLL_208M,
			      SND_SOC_NOPM, 0, 0,
			      mt_pll208m_vow_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("AUDGLB_VOW", SUPPLY_SEQ_AUD_GLB_VOW,
			      MT6338_VOWPLL_PMU_CON0,
			      RG_VOWPLL_EN_SFT, 0,
			      mt_vowpll_event,
			      SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY_S("VOW_DIG_CFG", SUPPLY_SEQ_VOW_DIG_CFG,
			      SND_SOC_NOPM,
			      0, 1,
			      mt_vow_digital_cfg_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
#endif
	/* Digital Clock */
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_AFE_CTL", SUPPLY_SEQ_AUD_TOP_LAST,
			      MT6338_AUDIO_TOP_CON0,
			      PDN_AFE_CTL_SFT, 1,
			      mt_audtop_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_DAC_CTL", SUPPLY_SEQ_AUD_TOP,
			      MT6338_AUDIO_TOP_CON0,
			      PDN_DAC_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_ADC_CTL", SUPPLY_SEQ_AUD_TOP,
			      MT6338_AUDIO_TOP_CON0,
			      PDN_ADC_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_ADDA6_ADC_CTL", SUPPLY_SEQ_AUD_TOP,
			      MT6338_AUDIO_TOP_CON0,
			      PDN_ADDA6_ADC_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_I2S_DL", SUPPLY_SEQ_AUD_TOP,
			      MT6338_AUDIO_TOP_CON0,
			      PDN_I2S_DL_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_PWR_CLK", SUPPLY_SEQ_AUD_TOP,
			      MT6338_AUDIO_TOP_CON0,
			      PWR_CLK_DIS_CTL_SFT, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_PDN_AFE_TESTMODEL", SUPPLY_SEQ_AUD_TOP,
			      SND_SOC_NOPM,
			      0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_PDN_RESERVED", SUPPLY_SEQ_AUD_TOP,
			      SND_SOC_NOPM,
			      0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AUDIO_TOP_PDN_OTHERS", SUPPLY_SEQ_AUD_TOP,
				  SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("SDM", SUPPLY_SEQ_DL_SDM,
			      SND_SOC_NOPM, 0, 0,
			      mt_sdm_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("SDM_3RD", SUPPLY_SEQ_DL_SDM,
			      SND_SOC_NOPM, 0, 0,
			      mt_sdm_3rd_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("NLE", SUPPLY_SEQ_DL_NLE,
			      SND_SOC_NOPM, 0, 0,
			      mt_nle_event,
			      SND_SOC_DAPM_PRE_PMU),

	/* ch123 share SDM FIFO CLK */
	SND_SOC_DAPM_SUPPLY_S("SDM_FIFO_CLK", SUPPLY_SEQ_DL_SDM_FIFO_CLK,
			      SND_SOC_NOPM, 0, 0,
			      mt_sdm_fifo_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("NCP", SUPPLY_SEQ_DL_NCP,
				  SND_SOC_NOPM, 0, 0, NULL,
				  SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_SUPPLY("DL Digital Clock", SND_SOC_NOPM,
			      0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DL Digital Clock CH_1_2", SND_SOC_NOPM,
			      0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DL Digital Clock CH_3_4", SND_SOC_NOPM,
			      0, 0, NULL, 0),

	/* AFE ON */
	SND_SOC_DAPM_SUPPLY_S("AFE_ON", SUPPLY_SEQ_AFE,
			      MT6338_AFE_TOP_CON0, AFE_ON_SFT, 0,
			      mt_top_event,
			      SND_SOC_DAPM_PRE_PMU),
	/* GPIO */
	SND_SOC_DAPM_SUPPLY_S("DL_GPIO", SUPPLY_SEQ_DL_GPIO,
			      SND_SOC_NOPM, 0, 0,
			      mt_dl_gpio_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("UL_GPIO", SUPPLY_SEQ_UL_GPIO,
			      SND_SOC_NOPM, 0, 0,
			      mt_ul_gpio_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* AIF Rx*/
	SND_SOC_DAPM_AIF_IN("AIF_RX", "AIF1 Playback", 0,
			      SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("AIF2_RX", "AIF2 Playback", 0,
			      SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SUPPLY_S("AFE_DL_SRC", SUPPLY_SEQ_DL_SRC,
			      SND_SOC_NOPM, 0, 0,
			      mt_dl_src_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("AFE_2ND_DL_SRC", SUPPLY_SEQ_DL_SRC,
			      SND_SOC_NOPM, 0, 0,
			      mt_2nd_dl_src_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* DL Supply */
	SND_SOC_DAPM_SUPPLY("DL Power Supply", SND_SOC_NOPM,
			      0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY_S("ESD_RESIST", SUPPLY_SEQ_DL_ESD_RESIST,
			      SND_SOC_NOPM, 0, 0,
			      mt_esd_resist_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("LDO", SUPPLY_SEQ_DL_LDO,
				  SND_SOC_NOPM, 0, 0,
			      mt_ldo_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("LDO_REMOTE", SUPPLY_SEQ_DL_LDO_REMOTE_SENSE,
			      SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("IBIST", SUPPLY_SEQ_DL_IBIST,
			      SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DAC */
	SND_SOC_DAPM_MUX("DAC In Mux", SND_SOC_NOPM, 0, 0, &dac_in_mux_control),

	SND_SOC_DAPM_DAC("DACL", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DAC("DACR", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DAC("DAC_3RD", NULL, SND_SOC_NOPM, 0, 0),

	/* Headphone */
	SND_SOC_DAPM_MUX_E("HPL Mux", SND_SOC_NOPM, 0, 0,
			      &hpl_in_mux_control,
			      mt_hp_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MUX_E("HPR Mux", SND_SOC_NOPM, 0, 0,
			      &hpr_in_mux_control,
			      mt_hp_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("HP_Supply", SND_SOC_NOPM,
			    0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("HP_PULL_DOWN", SUPPLY_SEQ_HP_PULL_DOWN,
			      SND_SOC_NOPM,
			      0, 0,
			      mt_hp_pull_down_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("HP_MUTE", SUPPLY_SEQ_HP_MUTE,
			      SND_SOC_NOPM,
			      0, 0,
			      mt_hp_mute_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("HP_DAMP", SUPPLY_SEQ_HP_DAMPING_OFF_RESET_CMFB,
			      SND_SOC_NOPM,
			      0, 0,
			      mt_hp_damp_event,
			      SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("HP_ANA_TRIM", SUPPLY_SEQ_HP_ANA_TRIM,
			      SND_SOC_NOPM,
			      0, 0,
			      mt_hp_ana_trim_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* Receiver */
	SND_SOC_DAPM_MUX_E("RCV Mux", SND_SOC_NOPM, 0, 0,
			      &rcv_in_mux_control,
			      mt_rcv_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	/* LOL */
	SND_SOC_DAPM_MUX_E("LOL Mux", SND_SOC_NOPM, 0, 0,
			      &lo_in_mux_control,
			      mt_lo_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	/* Outputs */
	SND_SOC_DAPM_OUTPUT("Receiver"),
	SND_SOC_DAPM_OUTPUT("Headphone L"),
	SND_SOC_DAPM_OUTPUT("Headphone R"),
	SND_SOC_DAPM_OUTPUT("Headphone L Ext Spk Amp"),
	SND_SOC_DAPM_OUTPUT("Headphone R Ext Spk Amp"),
	SND_SOC_DAPM_OUTPUT("LINEOUT L"),

	/* SGEN */
	//todo
	SND_SOC_DAPM_SUPPLY("SGEN DL Enable", MT6338_AFE_SINEGEN_CON0,
			      SINEGEN_DAC_EN_SFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("SGEN MUTE", SND_SOC_NOPM,
			      0, 0,
			      mt_sgen_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("SGEN DL SRC", SND_SOC_NOPM,
			      SND_SOC_NOPM, 0, NULL, 0),
	/* tricky, same reg/bit as "AIF_RX", reconsider */

	SND_SOC_DAPM_INPUT("SGEN DL"),

	/* Uplinks */
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0,
			      SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2TX", "AIF2 Capture", 0,
			      SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SUPPLY_S("ADC_CLKGEN", SUPPLY_SEQ_ADC_CLKGEN,
			      SND_SOC_NOPM, 0, 0,
			      mt_adc_clk_gen_event,
			      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY_S("DCC_CLK", SUPPLY_SEQ_DCC_CLK,
			      SND_SOC_NOPM, 0, 0,
			      mt_dcc_clk_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DCC_r_CLK", SUPPLY_SEQ_DCC_CLK,
			      SND_SOC_NOPM, 0, 0,
			      mt_r_dcc_clk_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DCC_3_CLK", SUPPLY_SEQ_DCC_CLK,
			      SND_SOC_NOPM, 0, 0,
			      mt_3_dcc_clk_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("DCC_4_CLK", SUPPLY_SEQ_DCC_CLK,
			      SND_SOC_NOPM, 0, 0,
			      mt_4_dcc_clk_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Uplinks MUX */
	SND_SOC_DAPM_MUX("AIF Out Mux", SND_SOC_NOPM, 0, 0,
			      &aif_out_mux_control),

	SND_SOC_DAPM_MUX("AIF2 Out Mux", SND_SOC_NOPM, 0, 0,
			      &aif2_out_mux_control),

	SND_SOC_DAPM_SUPPLY("AIFTX_Supply", MT6338_AFE_ADDA_UL_DL_CON0_0,
				  MT6338_ADDA_AFE_ON_SFT, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY_S("MTKAIF_TX", SUPPLY_SEQ_UL_MTKAIF,
			      SND_SOC_NOPM, 0, 0,
			      mt_mtkaif_tx_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("UL_SRC_CF", SUPPLY_SEQ_UL_SRC,
			      MT6338_AFE_ADDA_UL_SRC_CON0_3,
			      ADDA_ULCF_CFG_EN_CTL_SFT, 0,
				  mt_ulcf_event,
				  SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("UL_SRC", SUPPLY_SEQ_UL_SRC,
			      SND_SOC_NOPM, 0, 0,
			      mt_ul_src_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("UL_SRC_DMIC", SUPPLY_SEQ_UL_SRC_DMIC,
			      MT6338_AFE_ADDA_UL_DL_CON0_0,
			      MT6338_AFE_DMIC_CKDIV_ON_SFT, 0,
			      mt_ul_src_dmic_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("UL_SRC_34", SUPPLY_SEQ_UL_SRC,
				  SND_SOC_NOPM, 0, 0,
			      mt_ul34_src_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("UL_SRC_34_DMIC", SUPPLY_SEQ_UL_SRC_DMIC,
			      MT6338_AFE_ADDA_UL_DL_CON0_0,
			      MT6338_AFE_DMIC_CKDIV_ON_SFT, 0,
			      mt_ul_src_34_dmic_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("MISO0_MUX", SND_SOC_NOPM, 0, 0, &miso0_mux_control),
	SND_SOC_DAPM_MUX("MISO1_MUX", SND_SOC_NOPM, 0, 0, &miso1_mux_control),
	SND_SOC_DAPM_MUX("MISO2_MUX", SND_SOC_NOPM, 0, 0, &miso2_mux_control),
	SND_SOC_DAPM_MUX("MISO3_MUX", SND_SOC_NOPM, 0, 0, &miso3_mux_control),

	SND_SOC_DAPM_MUX("UL_SRC_MUX", SND_SOC_NOPM, 0, 0,
			      &ul_src_mux_control),
	SND_SOC_DAPM_MUX("UL2_SRC_MUX", SND_SOC_NOPM, 0, 0,
			      &ul2_src_mux_control),
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
	SND_SOC_DAPM_MUX("VOW_UL_SRC_MUX", SND_SOC_NOPM, 0, 0,
			      &vow_ul_src_mux_control),
#endif
	SND_SOC_DAPM_MUX("DMIC0_MUX", SND_SOC_NOPM, 0, 0, &dmic0_mux_control),
	SND_SOC_DAPM_MUX("DMIC1_MUX", SND_SOC_NOPM, 0, 0, &dmic1_mux_control),
	SND_SOC_DAPM_MUX("DMIC2_MUX", SND_SOC_NOPM, 0, 0, &dmic2_mux_control),
	SND_SOC_DAPM_MUX("DMIC3_MUX", SND_SOC_NOPM, 0, 0, &dmic3_mux_control),
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
	SND_SOC_DAPM_MUX("VOW_AMIC0_MUX", SND_SOC_NOPM, 0, 0,
			      &vow_amic0_mux_control),
	SND_SOC_DAPM_MUX("VOW_AMIC1_MUX", SND_SOC_NOPM, 0, 0,
			      &vow_amic1_mux_control),
	SND_SOC_DAPM_MUX("VOW_AMIC2_MUX", SND_SOC_NOPM, 0, 0,
			      &vow_amic2_mux_control),
	SND_SOC_DAPM_MUX("VOW_AMIC3_MUX", SND_SOC_NOPM, 0, 0,
			      &vow_amic3_mux_control),
#endif
	SND_SOC_DAPM_MUX_E("ADC_L_Mux", SND_SOC_NOPM, 0, 0,
			      &adc_left_mux_control, NULL, 0),
	SND_SOC_DAPM_MUX_E("ADC_R_Mux", SND_SOC_NOPM, 0, 0,
			      &adc_right_mux_control, NULL, 0),
	SND_SOC_DAPM_MUX_E("ADC_3_Mux", SND_SOC_NOPM, 0, 0,
			      &adc_3_mux_control, NULL, 0),
	SND_SOC_DAPM_MUX_E("ADC_4_Mux", SND_SOC_NOPM, 0, 0,
			      &adc_4_mux_control, NULL, 0),

	SND_SOC_DAPM_ADC("ADC_L", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC_R", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC_3", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC_4", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SUPPLY_S("ADC_L_EN", SUPPLY_SEQ_UL_ADC,
			      SND_SOC_NOPM, 0, 0,
			      mt_adc_l_event,
			      SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU |
			      SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ADC_R_EN", SUPPLY_SEQ_UL_ADC,
			      SND_SOC_NOPM, 0, 0,
			      mt_adc_r_event,
			      SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU |
			      SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ADC_3_EN", SUPPLY_SEQ_UL_ADC,
			      SND_SOC_NOPM, 0, 0,
			      mt_adc_3_event,
			      SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU |
			      SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ADC_4_EN", SUPPLY_SEQ_UL_ADC,
			      SND_SOC_NOPM, 0, 0,
			      mt_adc_4_event,
			      SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU |
			      SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("PGA_L_Mux", SND_SOC_NOPM, 0, 0,
			      &pga_left_mux_control,
			      mt_pga_l_mux_event,
			      SND_SOC_DAPM_WILL_PMU),
	SND_SOC_DAPM_MUX_E("PGA_R_Mux", SND_SOC_NOPM, 0, 0,
			      &pga_right_mux_control,
			      mt_pga_r_mux_event,
			      SND_SOC_DAPM_WILL_PMU),
	SND_SOC_DAPM_MUX_E("PGA_3_Mux", SND_SOC_NOPM, 0, 0,
			      &pga_3_mux_control,
			      mt_pga_3_mux_event,
			      SND_SOC_DAPM_WILL_PMU),
	SND_SOC_DAPM_MUX_E("PGA_4_Mux", SND_SOC_NOPM, 0, 0,
			      &pga_4_mux_control,
			      mt_pga_4_mux_event,
			      SND_SOC_DAPM_WILL_PMU),

	SND_SOC_DAPM_PGA("PGA_L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGA_R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGA_3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PGA_4", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY_S("PGA_L_EN", SUPPLY_SEQ_UL_PGA,
			      MT6338_AUDENC_PMU_CON0,
			      RG_AUDPREAMPLON_SFT, 0,
			      mt_pga_l_event,
			      SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU |
			      SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("PGA_R_EN", SUPPLY_SEQ_UL_PGA,
			      MT6338_AUDENC_PMU_CON2,
			      RG_AUDPREAMPRON_SFT, 0,
			      mt_pga_r_event,
			      SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU |
			      SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("PGA_3_EN", SUPPLY_SEQ_UL_PGA,
			      MT6338_AUDENC_PMU_CON4,
			      RG_AUDPREAMP3ON_SFT, 0,
			      mt_pga_3_event,
			      SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU |
			      SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("PGA_4_EN", SUPPLY_SEQ_UL_PGA,
			      MT6338_AUDENC_PMU_CON6,
			      RG_AUDPREAMP4ON_SFT, 0,
			      mt_pga_4_event,
			      SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU |
			      SND_SOC_DAPM_POST_PMD),
	/* UL input */
	SND_SOC_DAPM_INPUT("AIN0"),
	SND_SOC_DAPM_INPUT("AIN1"),
	SND_SOC_DAPM_INPUT("AIN2"),
	SND_SOC_DAPM_INPUT("AIN3"),
	SND_SOC_DAPM_INPUT("AIN4"),
	SND_SOC_DAPM_INPUT("AIN5"),
	SND_SOC_DAPM_INPUT("AIN6"),

	SND_SOC_DAPM_INPUT("AIN0_DMIC"),
	SND_SOC_DAPM_INPUT("AIN2_DMIC"),
	SND_SOC_DAPM_INPUT("AIN3_DMIC"),
	SND_SOC_DAPM_INPUT("AIN4_DMIC"),

	/* mic bias */
	SND_SOC_DAPM_SUPPLY_S("MIC_BIAS_0", SUPPLY_SEQ_MIC_BIAS,
			      MT6338_AUDENC_PMU_CON59,
			      RG_AUDPWDBMICBIAS0_SFT, 0,
			      mt_mic_bias_0_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("MIC_BIAS_1", SUPPLY_SEQ_MIC_BIAS,
			      MT6338_AUDENC_PMU_CON61,
			      RG_AUDPWDBMICBIAS1_SFT, 0,
			      mt_mic_bias_1_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("MIC_BIAS_2", SUPPLY_SEQ_MIC_BIAS,
				  MT6338_AUDENC_PMU_CON63,
			      RG_AUDPWDBMICBIAS2_SFT, 0,
			      mt_mic_bias_2_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("MIC_BIAS_3", SUPPLY_SEQ_MIC_BIAS,
			      MT6338_AUDENC_PMU_CON65,
			      RG_AUDPWDBMICBIAS3_SFT, 0,
			      mt_mic_bias_3_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* dmic */
	SND_SOC_DAPM_SUPPLY_S("DMIC_0", SUPPLY_SEQ_DMIC,
			      MT6338_AUDENC_PMU_CON55,
			      RG_AUDDIGMICEN_SFT, 0,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DMIC_1", SUPPLY_SEQ_DMIC,
			      MT6338_AUDENC_PMU_CON57,
			      RG_AUDDIGMIC1EN_SFT, 0,
			      NULL, 0),

	/* DC trim : trigger dc trim flow because set the reg when init_reg */
	/* this must be at the last widget */
	SND_SOC_DAPM_SUPPLY("DC Trim", MT6338_AUDDEC_PMU_CON28,
			    RG_AUDTRIMBUF_EN_VAUDP18_SFT, 0,
			    mt_dc_trim_event, SND_SOC_DAPM_POST_PMD),
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
	SND_SOC_DAPM_AIF_OUT_E("VOW TX", "VOW Capture", 0,
			       SND_SOC_NOPM, 0, 0,
			       NULL,
			       SND_SOC_DAPM_WILL_PMU |
			       SND_SOC_DAPM_PRE_PMU |
			       SND_SOC_DAPM_POST_PMD),
#endif
};

#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
static int mt_vow_amic_connect(struct snd_soc_dapm_widget *source,
			       struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (IS_AMIC_BASE(priv->mux_select[MUX_MIC_TYPE_0]) ||
	    IS_AMIC_BASE(priv->mux_select[MUX_MIC_TYPE_1]) ||
	    IS_AMIC_BASE(priv->mux_select[MUX_MIC_TYPE_2]) ||
	    IS_AMIC_BASE(priv->mux_select[MUX_MIC_TYPE_3]))
		return 1;
	else
		return 0;
}
#endif

static int mt_dcc_clk_connect(struct snd_soc_dapm_widget *source,
			      struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = sink;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (IS_DCC_BASE(priv->mux_select[MUX_MIC_TYPE_0]) ||
	    IS_DCC_BASE(priv->mux_select[MUX_MIC_TYPE_1]) ||
	    IS_DCC_BASE(priv->mux_select[MUX_MIC_TYPE_2]) ||
	    IS_DCC_BASE(priv->mux_select[MUX_MIC_TYPE_3]))
		return 1;
	else
		return 0;
}

/* DAPM Route */
static const struct snd_soc_dapm_route mt6338_dapm_routes[] = {
	/* Capture */
	{"AIFTX_Supply", NULL, "UL_GPIO"},
	/* {"AIFTX_Supply", NULL, "KEY"}, */
	{"AIFTX_Supply", NULL, "CLK_BUF"},
	{"AIFTX_Supply", NULL, "AUDGLB"},
	{"AIFTX_Supply", NULL, "CLKSQ Audio"},
	{"AIFTX_Supply", NULL, "PLL18 EN", is_need_pll_208M},
	{"AIFTX_Supply", NULL, "PLL18 Audio", is_need_pll_208M},
	{"AIFTX_Supply", NULL, "AUD_CK"},
	{"AIFTX_Supply", NULL, "AUDIF_CK"},
	{"AIFTX_Supply", NULL, "SRAM Audio"},
	{"AIFTX_Supply", NULL, "AUDIO_TOP_AFE_CTL"},
	{"AIFTX_Supply", NULL, "AUDIO_TOP_PWR_CLK"},
	/*
	 * *_ADC_CTL should enable only if UL_SRC in use,
	 * but dm ck may be needed even UL_SRC_x not in use
	 */
	{"AIFTX_Supply", NULL, "AUDIO_TOP_ADC_CTL"},
	{"AIFTX_Supply", NULL, "AUDIO_TOP_ADDA6_ADC_CTL"},
	{"AIFTX_Supply", NULL, "AFE_ON"},

	/* ul ch 12 */
	{"AIF1TX", NULL, "AIF Out Mux"},
	{"AIF1TX", NULL, "AIFTX_Supply"},
	{"AIF1TX", NULL, "MTKAIF_TX"},

	{"AIF2TX", NULL, "AIF2 Out Mux"},
	{"AIF2TX", NULL, "AIFTX_Supply"},
	{"AIF2TX", NULL, "MTKAIF_TX"},

	{"AIF Out Mux", "Normal Path", "MISO0_MUX"},
	{"AIF Out Mux", "Normal Path", "MISO1_MUX"},

	{"AIF2 Out Mux", "Normal Path", "MISO2_MUX"},
	{"AIF2 Out Mux", "Normal Path", "MISO3_MUX"},

	{"MISO0_MUX", "UL1_CH1", "UL_SRC_MUX"},
	{"MISO0_MUX", "UL1_CH2", "UL_SRC_MUX"},
	{"MISO0_MUX", "UL2_CH1", "UL2_SRC_MUX"},
	{"MISO0_MUX", "UL2_CH2", "UL2_SRC_MUX"},

	{"MISO1_MUX", "UL1_CH1", "UL_SRC_MUX"},
	{"MISO1_MUX", "UL1_CH2", "UL_SRC_MUX"},
	{"MISO1_MUX", "UL2_CH1", "UL2_SRC_MUX"},
	{"MISO1_MUX", "UL2_CH2", "UL2_SRC_MUX"},

	{"MISO2_MUX", "UL1_CH1", "UL_SRC_MUX"},
	{"MISO2_MUX", "UL1_CH2", "UL_SRC_MUX"},
	{"MISO2_MUX", "UL2_CH1", "UL2_SRC_MUX"},
	{"MISO2_MUX", "UL2_CH2", "UL2_SRC_MUX"},

	{"MISO3_MUX", "UL1_CH1", "UL_SRC_MUX"},
	{"MISO3_MUX", "UL1_CH2", "UL_SRC_MUX"},
	{"MISO3_MUX", "UL2_CH1", "UL2_SRC_MUX"},
	{"MISO3_MUX", "UL2_CH2", "UL2_SRC_MUX"},

	{"UL_SRC_MUX", "AMIC", "ADC_L"},
	{"UL_SRC_MUX", "AMIC", "ADC_R"},
	{"UL_SRC_MUX", "DMIC", "DMIC0_MUX"},
	{"UL_SRC_MUX", "DMIC", "DMIC1_MUX"},
	{"UL_SRC_MUX", NULL, "UL_SRC_CF", is_need_ulcf},
	{"UL_SRC_MUX", NULL, "UL_SRC"},

	{"UL2_SRC_MUX", "AMIC", "ADC_3"},
	{"UL2_SRC_MUX", "AMIC", "ADC_4"},
	{"UL2_SRC_MUX", "DMIC", "DMIC2_MUX"},
	{"UL2_SRC_MUX", "DMIC", "DMIC3_MUX"},
	{"UL2_SRC_MUX", NULL, "UL_SRC_34"},

	{"DMIC0_MUX", "DMIC_DATA0", "AIN0_DMIC"},
	{"DMIC0_MUX", "DMIC_DATA1", "AIN2_DMIC"},
	{"DMIC0_MUX", "DMIC_DATA2", "AIN3_DMIC"},
	{"DMIC0_MUX", "DMIC_DATA3", "AIN4_DMIC"},
	{"DMIC1_MUX", "DMIC_DATA0", "AIN0_DMIC"},
	{"DMIC1_MUX", "DMIC_DATA1", "AIN2_DMIC"},
	{"DMIC1_MUX", "DMIC_DATA2", "AIN3_DMIC"},
	{"DMIC1_MUX", "DMIC_DATA3", "AIN4_DMIC"},
	{"DMIC2_MUX", "DMIC_DATA0", "AIN0_DMIC"},
	{"DMIC2_MUX", "DMIC_DATA1", "AIN2_DMIC"},
	{"DMIC2_MUX", "DMIC_DATA2", "AIN3_DMIC"},
	{"DMIC2_MUX", "DMIC_DATA3", "AIN4_DMIC"},
	{"DMIC3_MUX", "DMIC_DATA0", "AIN0_DMIC"},
	{"DMIC3_MUX", "DMIC_DATA1", "AIN2_DMIC"},
	{"DMIC3_MUX", "DMIC_DATA2", "AIN3_DMIC"},
	{"DMIC3_MUX", "DMIC_DATA3", "AIN4_DMIC"},

	{"DMIC0_MUX", NULL, "UL_SRC_DMIC"},
	{"DMIC1_MUX", NULL, "UL_SRC_DMIC"},
	{"DMIC2_MUX", NULL, "UL_SRC_DMIC"},
	{"DMIC3_MUX", NULL, "UL_SRC_DMIC"},
	{"DMIC0_MUX", NULL, "UL_SRC_34_DMIC"},
	{"DMIC1_MUX", NULL, "UL_SRC_34_DMIC"},
	{"DMIC2_MUX", NULL, "UL_SRC_34_DMIC"},
	{"DMIC3_MUX", NULL, "UL_SRC_34_DMIC"},

	{"AIN0_DMIC", NULL, "DMIC_0"},
	{"AIN2_DMIC", NULL, "DMIC_0"},
	{"AIN3_DMIC", NULL, "DMIC_1"},
	{"AIN4_DMIC", NULL, "DMIC_1"},
	{"AIN0_DMIC", NULL, "MIC_BIAS_0"},
	{"AIN2_DMIC", NULL, "MIC_BIAS_0"},
	{"AIN3_DMIC", NULL, "MIC_BIAS_2"},
	{"AIN4_DMIC", NULL, "MIC_BIAS_2"},
	/* adc */
	{"ADC_L", NULL, "ADC_L_Mux"},
	{"ADC_L", NULL, "ADC_CLKGEN"},
	{"ADC_L", NULL, "ADC_L_EN"},
	{"ADC_R", NULL, "ADC_R_Mux"},
	{"ADC_R", NULL, "ADC_CLKGEN"},
	{"ADC_R", NULL, "ADC_R_EN"},
	/*
	 * amic fifo ch1/2 clk from ADC_L,
	 * enable ADC_L even use ADC_R only
	 */
	{"ADC_R", NULL, "ADC_L_EN"},
	{"ADC_3", NULL, "ADC_3_Mux"},
	{"ADC_3", NULL, "ADC_CLKGEN"},
	{"ADC_3", NULL, "ADC_3_EN"},

	{"ADC_4", NULL, "ADC_4_Mux"},
	{"ADC_4", NULL, "ADC_CLKGEN"},
	{"ADC_4", NULL, "ADC_4_EN"},

	{"ADC_L_Mux", "Left Preamplifier", "PGA_L"},
	{"ADC_R_Mux", "Right Preamplifier", "PGA_R"},
	{"ADC_3_Mux", "Preamplifier", "PGA_3"},
	{"ADC_4_Mux", "Preamplifier", "PGA_4"},

	{"PGA_L", NULL, "PGA_L_Mux"},
	{"PGA_L", NULL, "PGA_L_EN"},

	{"PGA_R", NULL, "PGA_R_Mux"},
	{"PGA_R", NULL, "PGA_R_EN"},

	{"PGA_3", NULL, "PGA_3_Mux"},
	{"PGA_3", NULL, "PGA_3_EN"},

	{"PGA_4", NULL, "PGA_4_Mux"},
	{"PGA_4", NULL, "PGA_4_EN"},

	{"PGA_L", NULL, "DCC_CLK", mt_dcc_clk_connect},
	{"PGA_R", NULL, "DCC_r_CLK", mt_dcc_clk_connect},
	{"PGA_3", NULL, "DCC_3_CLK", mt_dcc_clk_connect},
	{"PGA_4", NULL, "DCC_4_CLK", mt_dcc_clk_connect},

	{"PGA_L_Mux", "AIN0", "AIN0"},
	{"PGA_L_Mux", "AIN1", "AIN1"},
	{"PGA_L_Mux", "AIN2", "AIN2"},

	{"PGA_R_Mux", "AIN0", "AIN0"},
	{"PGA_R_Mux", "AIN1", "AIN1"},
	{"PGA_R_Mux", "AIN2", "AIN2"},
	{"PGA_3_Mux", "AIN0", "AIN0"},

	{"PGA_3_Mux", "AIN2", "AIN2"},
	{"PGA_3_Mux", "AIN3", "AIN3"},
	{"PGA_3_Mux", "AIN5", "AIN5"},

	{"PGA_4_Mux", "AIN2", "AIN2"},
	{"PGA_4_Mux", "AIN3", "AIN3"},
	{"PGA_4_Mux", "AIN4", "AIN4"},
	{"PGA_4_Mux", "AIN6", "AIN6"},

	{"AIN0", NULL, "MIC_BIAS_0"},
	{"AIN1", NULL, "MIC_BIAS_1"},
	{"AIN2", NULL, "MIC_BIAS_2"},
	{"AIN3", NULL, "MIC_BIAS_3"},
	{"AIN4", NULL, "MIC_BIAS_3"},
	{"AIN5", NULL, "MIC_BIAS_0"},
	{"AIN6", NULL, "MIC_BIAS_0"},

	/* DL Supply */
	{"DL Power Supply", NULL, "DL_GPIO"},
	/* {"DL Power Supply", NULL, "KEY"}, */
	{"DL Power Supply", NULL, "CLK_BUF"},
	/* {"DL Power Supply", NULL, "vaud18"}, */
	{"DL Power Supply", NULL, "NCP_CK"},
	{"DL Power Supply", NULL, "NCP_CK_208"},
	{"DL Power Supply", NULL, "LDO_VAUD18"},
	{"DL Power Supply", NULL, "CLKSQ Audio"},
	{"DL Power Supply", NULL, "PLL18 EN", is_need_pll_208M},
	{"DL Power Supply", NULL, "PLL18 Audio", is_need_pll_208M},
	{"DL Power Supply", NULL, "AUDNCP_CK"},
	{"DL Power Supply", NULL, "ZCD13M_CK"},
	{"DL Power Supply", NULL, "AUD_CK"},
	{"DL Power Supply", NULL, "AUDIF_CK"},
	{"DL Power Supply", NULL, "AUD208M", is_need_pll_208M},
	{"DL Power Supply", NULL, "SRAM Audio"},

	/* DL Digital Supply */
	{"DL Digital Clock", NULL, "AUDIO_TOP_AFE_CTL"},
	{"DL Digital Clock", NULL, "AUDIO_TOP_DAC_CTL"},
	{"DL Digital Clock", NULL, "AUDIO_TOP_PWR_CLK"},
	{"DL Digital Clock", NULL, "SDM_FIFO_CLK"},
	{"DL Digital Clock", NULL, "NCP"},
	{"DL Digital Clock", NULL, "AFE_ON"},

	{"DL Digital Clock CH_1_2", NULL, "DL Digital Clock"},
	{"DL Digital Clock CH_1_2", NULL, "SDM"},
	{"DL Digital Clock CH_1_2", NULL, "SDM_3RD", is_hp_lowpower},
	{"DL Digital Clock CH_1_2", NULL, "AFE_DL_SRC"},
	{"DL Digital Clock CH_1_2", NULL, "AFE_2ND_DL_SRC", is_hp_lowpower},
	{"DL Digital Clock CH_1_2", NULL, "NLE"},
	{"DL Digital Clock CH_1_2", NULL, "ESD_RESIST"},
	{"DL Digital Clock CH_1_2", NULL, "AUDGLB"},
	{"DL Digital Clock CH_1_2", NULL, "LDO"},

	{"DL Digital Clock CH_3_4", NULL, "DL Digital Clock"},
	{"DL Digital Clock CH_3_4", NULL, "SDM_3RD"},
	{"DL Digital Clock CH_3_4", NULL, "AFE_2ND_DL_SRC"},
	{"DL Digital Clock CH_3_4", NULL, "ESD_RESIST"},
	{"DL Digital Clock CH_3_4", NULL, "AUDGLB"},
	{"DL Digital Clock CH_3_4", NULL, "LDO"},

	{"AIF_RX", NULL, "DL Digital Clock CH_1_2"},

	{"AIF2_RX", NULL, "DL Digital Clock CH_3_4"},

	/* DL Path */
	{"DAC In Mux", "Normal Path", "AIF_RX"},
	{"DAC In Mux", "Sgen", "SGEN DL"},
	{"SGEN DL", NULL, "SGEN DL SRC"},
	{"SGEN DL", NULL, "SGEN MUTE"},
	{"SGEN DL", NULL, "SGEN DL Enable"},
	{"SGEN DL", NULL, "DL Digital Clock CH_1_2"},
	{"SGEN DL", NULL, "DL Digital Clock CH_3_4"},
	/* {"SGEN DL", NULL, "AUDIO_TOP_PDN_AFE_TESTMODEL"},*/

	{"DACL", NULL, "DAC In Mux"},
	{"DACL", NULL, "DL Power Supply"},

	{"DACR", NULL, "DAC In Mux"},
	{"DACR", NULL, "DL Power Supply"},

	/* DAC 3RD */
	{"DAC In Mux", "Normal Path", "AIF2_RX"},
	{"DAC_3RD", NULL, "DAC In Mux"},
	{"DAC_3RD", NULL, "DL Power Supply"},

	/* Lineout Path */
	{"LOL Mux", "Playback", "DAC_3RD"},
	{"LOL Mux", "Playback_L_DAC", "DACL"},
	{"LINEOUT L", NULL, "LOL Mux"},

	/* Headphone Path */
	{"HP_Supply", NULL, "HP_ANA_TRIM"},
	{"HPL Mux", NULL, "HP_Supply"},
	{"HPR Mux", NULL, "HP_Supply"},

	{"HPL Mux", "Audio Playback", "DACL"},
	{"HPR Mux", "Audio Playback", "DACR"},
	{"HPL Mux", "HP Impedance", "DACL"},
	{"HPR Mux", "HP Impedance", "DACR"},
	{"HPL Mux", "LoudSPK Playback", "DACL"},
	{"HPR Mux", "LoudSPK Playback", "DACR"},

	{"Headphone L", NULL, "HPL Mux"},
	{"Headphone R", NULL, "HPR Mux"},
	{"Headphone L Ext Spk Amp", NULL, "HPL Mux"},
	{"Headphone R Ext Spk Amp", NULL, "HPR Mux"},

	/* Receiver Path */
	{"RCV Mux", "Voice Playback", "DACL"},
	{"Receiver", NULL, "RCV Mux"},
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
	{"VOW TX", NULL, "VOW_UL_SRC_MUX"},
	{"VOW TX", NULL, "KEY"},
	{"VOW TX", NULL, "AUDGLB"},
	{"VOW TX", NULL, "PLL18 EN"},
	{"VOW TX", NULL, "PLL18_VOW"},
	{"VOW TX", NULL, "SRAM Audio"},
	{"VOW TX", NULL, "AUDGLB_VOW", mt_vow_amic_connect},
	{"VOW TX", NULL, "AUD_CK", mt_vow_amic_connect},
	{"VOW TX", NULL, "VOW_DIG_CFG"},
	{"VOW_UL_SRC_MUX", "AMIC", "VOW_AMIC0_MUX"},
	{"VOW_UL_SRC_MUX", "AMIC", "VOW_AMIC1_MUX"},
	{"VOW_UL_SRC_MUX", "AMIC", "VOW_AMIC2_MUX"},
	{"VOW_UL_SRC_MUX", "AMIC", "VOW_AMIC3_MUX"},
	{"VOW_UL_SRC_MUX", "DMIC", "DMIC0_MUX"},
	{"VOW_UL_SRC_MUX", "DMIC", "DMIC1_MUX"},
	{"VOW_UL_SRC_MUX", "DMIC", "DMIC2_MUX"},
	{"VOW_UL_SRC_MUX", "DMIC", "DMIC3_MUX"},
	{"VOW_AMIC0_MUX", "ADC_DATA_0", "ADC_L"},
	{"VOW_AMIC0_MUX", "ADC_DATA_1", "ADC_R"},
	{"VOW_AMIC0_MUX", "ADC_DATA_2", "ADC_3"},
	{"VOW_AMIC0_MUX", "ADC_DATA_3", "ADC_4"},
	{"VOW_AMIC1_MUX", "ADC_DATA_0", "ADC_L"},
	{"VOW_AMIC1_MUX", "ADC_DATA_1", "ADC_R"},
	{"VOW_AMIC1_MUX", "ADC_DATA_2", "ADC_3"},
	{"VOW_AMIC1_MUX", "ADC_DATA_3", "ADC_4"},
	{"VOW_AMIC2_MUX", "ADC_DATA_0", "ADC_L"},
	{"VOW_AMIC2_MUX", "ADC_DATA_1", "ADC_R"},
	{"VOW_AMIC2_MUX", "ADC_DATA_2", "ADC_3"},
	{"VOW_AMIC2_MUX", "ADC_DATA_3", "ADC_4"},
	{"VOW_AMIC3_MUX", "ADC_DATA_0", "ADC_L"},
	{"VOW_AMIC3_MUX", "ADC_DATA_1", "ADC_R"},
	{"VOW_AMIC3_MUX", "ADC_DATA_2", "ADC_3"},
	{"VOW_AMIC3_MUX", "ADC_DATA_3", "ADC_4"},
#endif
};

static int mt6338_codec_dai_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params,
				      struct snd_soc_dai *dai)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int rate = params_rate(params);
	int id = dai->id;

	dev_info(priv->dev, "%s(), id %d, substream->stream %d, rate %d, number %d\n",
		 __func__, id, substream->stream, rate, substream->number);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		priv->dl_rate[id] = rate;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		priv->ul_rate[id] = rate;

	return 0;
}

static int mt6338_codec_dai_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s stream %d, dai id %d\n",
		 __func__, substream->stream, dai->id);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mt6338_set_playback_gpio(priv);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		mt6338_set_capture_gpio(priv);

	if (dai->id == MT6338_AIF_VOW) {
		priv->vow_enable = 1;
		mt6338_set_vow_gpio(priv);
	}

	return 0;
}

static const struct snd_soc_dai_ops mt6338_codec_dai_ops = {
	.hw_params = mt6338_codec_dai_hw_params,
	.startup = mt6338_codec_dai_startup,
};

static int mt6338_codec_dai_vow_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params,
				      struct snd_soc_dai *dai)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	unsigned int channel = params_channels(params);

	dev_info(priv->dev, "%s(), substream->stream %d, channel %d, number %d\n",
		 __func__,
		 substream->stream,
		 channel,
		 substream->number);

	priv->vow_channel = channel;

	return 0;
}

static int mt6338_codec_dai_vow_startup(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	priv->vow_enable = 1;

	return 0;
}

static void mt6338_codec_dai_vow_shutdown(struct snd_pcm_substream *substream,
				      struct snd_soc_dai *dai)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	dev_info(priv->dev, "%s stream %d\n", __func__, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mt6338_reset_playback_gpio(priv);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		mt6338_reset_capture_gpio(priv);

	if (dai->id == MT6338_AIF_VOW) {
		priv->vow_enable = 0;
		mt6338_reset_vow_gpio(priv);
	}
}

static const struct snd_soc_dai_ops mt6338_codec_dai_vow_ops = {
	.hw_params = mt6338_codec_dai_vow_hw_params,
	.startup = mt6338_codec_dai_vow_startup,
	.shutdown = mt6338_codec_dai_vow_shutdown,
};

#define MT6338_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |\
			SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_U16_BE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |\
			SNDRV_PCM_FMTBIT_U24_LE | SNDRV_PCM_FMTBIT_U24_BE |\
			SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE |\
			SNDRV_PCM_FMTBIT_U32_LE | SNDRV_PCM_FMTBIT_U32_BE)

static struct snd_soc_dai_driver mt6338_dai_driver[] = {
#if defined(MTKAIFV4_SUPPORT)
	{
		.id = MT6338_AIF_1,
		.name = "mt6338-snd-codec-aif1",
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000 |
				 SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = MT6338_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000 |
				 SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = MT6338_FORMATS,
		},
		.ops = &mt6338_codec_dai_ops,
	},
#else
	{
		.id = MT6338_AIF_1,
		.name = "mt6338-snd-codec-aif1-p",
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000 |
				 SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = MT6338_FORMATS,
		},
		.ops = &mt6338_codec_dai_ops,
	},
	{
		.id = MT6338_AIF_1,
		.name = "mt6338-snd-codec-aif1-c",
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000 |
				 SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = MT6338_FORMATS,
		},
		.ops = &mt6338_codec_dai_ops,
	},
#endif
	{
		.id = MT6338_AIF_2,
		.name = "mt6338-snd-codec-aif2",
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000 |
				 SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_192000,
			.formats = MT6338_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = MT6338_FORMATS,
		},
		.ops = &mt6338_codec_dai_ops,
	},
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
	{
		.id = MT6338_AIF_VOW,
		.name = "mt6338-snd-codec-vow",
		.capture = {
			.stream_name = "VOW Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_16000,
			.formats = MT6338_FORMATS,
		},
		.ops = &mt6338_codec_dai_vow_ops,
	},
#endif
};

/* dc trim */
static int mt6338_get_hpofs_auxadc(struct mt6338_priv *priv)
{
	int value = 0;
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int ret;
	struct iio_channel *auxadc = priv->hpofs_cal_auxadc;

	regmap_update_bits(priv->regmap, MT6338_AUXADC_RQST0,
			0x4, 0x4);
	if (!IS_ERR(auxadc)) {
		ret = iio_read_channel_raw(auxadc, &value);
		if (ret < 0) {
			dev_err(priv->dev, "Error: %s read fail (%d)\n",
				__func__, ret);
			return ret;
		}
	}
	dev_dbg(priv->dev, "%s read value (%d)\n",
			 __func__, value);
#endif /* #if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING) */
	return value;
}

int mt6338_enable_dc_compensation(struct mt6338_priv *priv, bool enable)
{
	regmap_update_bits(priv->regmap,
		MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_L,
		MT6338_AFE_DL_AUD_DC_COMP_EN_MASK_SFT,
			   (enable ? 1 : 0) << MT6338_AFE_DL_AUD_DC_COMP_EN_SFT);
	return 0;
}

int mt6338_set_lch_dc_compensation(struct mt6338_priv *priv, int value)
{
	regmap_write(priv->regmap,
		MT6338_AFE_ADDA_DL_DC_COMP_CFG0_H, ((value >> 8) & 0xff));
	regmap_write(priv->regmap,
		MT6338_AFE_ADDA_DL_DC_COMP_CFG0_M, (value & 0xff));
	return 0;
}

int mt6338_set_rch_dc_compensation(struct mt6338_priv *priv, int value)
{
	regmap_write(priv->regmap,
		MT6338_AFE_ADDA_DL_DC_COMP_CFG1_H, (value >> 8));
	regmap_write(priv->regmap,
		MT6338_AFE_ADDA_DL_DC_COMP_CFG1_M, (value & 0xff));
	return 0;
}

int mt6338_adda_dl_gain_control(struct mt6338_priv *priv, bool mute)
{
	unsigned int dl_2_gain_ctl;

	if (mute)
		dl_2_gain_ctl = MT6338_DL_GAIN_MUTE;
	else
		dl_2_gain_ctl = MT6338_DL_GAIN_NORMAL;

	/* Step2: DL digital gain control, 0x1800, (-19dB) */
	regmap_write(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON1_H,
		(dl_2_gain_ctl >> 8));
	regmap_write(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON1_M,
		(dl_2_gain_ctl & 0xff));

	return 0;
}
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static void set_trim_buf_in_mux(struct mt6338_priv *priv, int mux)
{
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON28,
		RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP18_MASK_SFT,
		mux << RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP18_SFT);
}
#endif
static void enable_trim_buf(struct mt6338_priv *priv, bool enable)
{
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON28,
		RG_AUDTRIMBUF_EN_VAUDP18_MASK_SFT,
		(enable ? 1 : 0) << RG_AUDTRIMBUF_EN_VAUDP18_SFT);
}

static void prepare_enable_trim_buf(struct mt6338_priv *priv, bool enable,
							   unsigned int gain, int mux)
{
	unsigned int value = 0;

	value = gain << RG_AUDTRIMBUF_GAINSEL_VAUDP18_SFT |
			mux << RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP18_SFT |
			(enable ? 1 : 0) << RG_AUDTRIMBUF_EN_VAUDP18_SFT;

	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON28, value);
}

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static void enable_trim_circuit(struct mt6338_priv *priv, bool enable)
{
	if (enable) {
		regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_CON0,
			RG_LDO_VAUD18_EN_0_MASK_SFT,
			0x1 << RG_LDO_VAUD18_EN_0_SFT);

		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON11,
			RG_AUDHPTRIM_EN_VAUDP18_MASK_SFT,
			0x1 << RG_AUDHPTRIM_EN_VAUDP18_SFT);

	} else {

		regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON11,
			RG_AUDHPTRIM_EN_VAUDP18_MASK_SFT,
			0x0 << RG_AUDHPTRIM_EN_VAUDP18_SFT);

		regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_CON0,
			RG_LDO_VAUD18_EN_0_MASK_SFT,
			0x0 << RG_LDO_VAUD18_EN_0_SFT);
	}
}

static void start_trim_hardware(struct mt6338_priv *priv)
{
	dev_info(priv->dev, "%s(), ++\n", __func__);

	/* Set playback gpio (mosi/clk/sync) */
	mt6338_set_playback_gpio(priv);
	/* XO_AUDIO_EN_M Enable */
	mt6338_set_dcxo(priv, true);

	mt6338_set_pmu(priv, true);

	/* Enable CLKSQ */
	mt6338_set_clksq(priv, true);
	mt6338_set_dl_src(priv, true);

	/* Turn on AUDNCP_CLKDIV engine clock */
	mt6338_set_topck(priv, true);

	/* release  aud 208M PD */
	regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H,
		RG_AUD208M_CK_PDN_MASK_SFT,
		0x0 << RG_AUD208M_CK_PDN_SFT);
	/* release SRAM  power down */
	regmap_write(priv->regmap, MT6338_AUD_TOP_SRAM_CON, 0x0);
	usleep_range(250, 270);

	/* Audio system digital clock power down release */
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON0, 0x0);
	usleep_range(250, 270);

	mt6338_set_aud_top(priv, true);

	/* sdm audio fifo clock power on */
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON2_L, 0x6);
	/* scrambler clock on enable */
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON0_H, 0xcb);
	/* scrambler clock on enable */
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON0_L, 0xa0);
	/* scrambler enable */
		regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON2_H, 0x70);
	/* sdm power on */
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON2_L, 0xb);
	/* select 12bit 2nd SDM  */
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_H,
		AFE_DL_USE_NEW_2ND_12BIT_SDM_MASK_SFT,
		0x1 << AFE_DL_USE_NEW_2ND_12BIT_SDM_SFT);
	/* select FS = 48KHz */
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0_H,
		AFE_DL_INPUT_MODE_CTL_MASK_SFT,
		MT6338_DLSRC_48000HZ << AFE_DL_INPUT_MODE_CTL_SFT);

	/* afe enable, dl_lr_swap = 0, ul_lr_swap = 0 */
	regmap_update_bits(priv->regmap, MT6338_AFE_TOP_CON0,
		AFE_ON_MASK_SFT,
		0x1 << AFE_ON_SFT);
	/* turn on DL SRC */
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0,
		AFE_DL_SRC_ON_TMP_CTL_PRE_MASK_SFT,
		0x1 << AFE_DL_SRC_ON_TMP_CTL_PRE_SFT);

	/* Reduce ESD resistance of AU_REFN */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON10,
		RG_AUDREFN_DERES_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDREFN_DERES_EN_VAUDP18_SFT);

	/* Enable AUDGLB */
	mt6338_set_aud_global_bias(priv, true);

	/* mtk_hp_enable */
	ldo_select_to_min(priv, true, true);
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x7);

	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG1, 0xCb);
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG0, 0x1);
	nvreg_select_to_min(priv, true, true);

	/* Enable AUD_CLK */
	mt6338_set_decoder_clk(priv, true, true);

	/* Enable HP mute */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPLMUTE_EN_VAUDP18_MASK_SFT |
		RG_HPRMUTE_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLMUTE_EN_VAUDP18_SFT);
	usleep_range(250, 270);

	hp_pull_down(priv, false, true);

	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON3, 0xe3);

	/* Set HP NREG segmentation to min */
	regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON12, 0x0);
	/* Set HP Trim code mode */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON19, 0x0c);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON11,
		RG_AUDHPTRIM_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPTRIM_EN_VAUDP18_SFT);

	/* Disable shortcut */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLSCDISABLE_VAUDP18_MASK_SFT |
		RG_AUDHPRSCDISABLE_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLSCDISABLE_VAUDP18_SFT);

	/* BAIS */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON31,
		RG_AUDIBIASPWRDN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDIBIASPWRDN_VAUDP18_SFT);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON30,
		RG_AUDBIASADJ_0_HP_VAUDP18_MASK_SFT,
		DRBIAS_8UA << RG_AUDBIASADJ_0_HP_VAUDP18_SFT);

	/* Set HP & ZCD bias current optimization */
	/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON32,
		IBIAS_ZCD_MASK_SFT,
		IBIAS_ZCD_4UA << IBIAS_ZCD_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON32,
		IBIAS_HP_MASK_SFT,
		IBIAS_5UA << IBIAS_HP_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON32,
		IBIAS_HS_MASK_SFT,
		IBIAS_5UA << IBIAS_HS_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON32,
		IBIAS_LO_MASK_SFT,
		IBIAS_5UA << IBIAS_LO_SFT);

	/* NLE */
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_H,
		RG_DG_OUTPUT_DEBUG_MODE_LCH_MASK_SFT,
		0x1 << RG_DG_OUTPUT_DEBUG_MODE_LCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_H,
		RG_DG_OUTPUT_DEBUG_MODE_RCH_MASK_SFT,
		0x1 << RG_DG_OUTPUT_DEBUG_MODE_RCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H,
		RG_D2A_SIGNAL_SW_DEBUG_MODE_RCH_MASK_SFT,
		0x1 << RG_D2A_SIGNAL_SW_DEBUG_MODE_RCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L,
		RG_D2A_SIGNAL_SW_DEBUG_MODE_LCH_MASK_SFT,
		0x1 << RG_D2A_SIGNAL_SW_DEBUG_MODE_LCH_SFT);

	/* Set HPP/N STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x7 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x7 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);

	/* HP Feedback Cap select 2'b00: 15pF */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_AUDHPHFCOMPBUFGAINSEL_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPHFCOMPBUFGAINSEL_VAUDP18_SFT);

	/* Enable HP De-CMgain circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
		RG_AUDHPMAINCM_COMP_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPMAINCM_COMP_EN_VAUDP18_SFT);
	/* Disable 2nd order damp circuit when turn on sequence */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_DAMP2ND_EN_VAUDP18_MASK_SFT,
		0x0 << RG_DAMP2ND_EN_VAUDP18_SFT);

	/* apply volume setting */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_HPLHDRM_PFL_EN_VAUDP18_MASK_SFT |
		RG_HPRHDRM_PFL_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLHDRM_PFL_EN_VAUDP18_SFT);


	/* Set HP NREG segmentation to default value */
	regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON12, 0x70);
	/* Enable Neg R when turn on sequence */
	regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON12_H,
		DA_AUDHPLNEGR_EN_VAUDP18_MASK_SFT |
		DA_AUDHPRNEGR_EN_VAUDP18_MASK_SFT,
		0x3 << DA_AUDHPLNEGR_EN_VAUDP18_SFT);

	/*Enable HPR/L main output stage to min*/
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
		RG_DA_HP_OUTSTG_RCH_MASK_SFT,
		0x0 << RG_DA_HP_OUTSTG_RCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
		RG_DA_HP_OUTSTG_LCH_MASK_SFT,
		0x0 << RG_DA_HP_OUTSTG_LCH_SFT);
	/* Damping adjustment select. (Hi-Fi mode) */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
		RG_AUDHPDAMP_ADJ_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPDAMP_ADJ_VAUDP18_SFT);
	/* Enable HP damping ckt.  */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_AUDHPDAMP_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPDAMP_EN_VAUDP18_SFT);

	/* Enable HFOP circuits for Hi-Fi mode */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
		RG_AUDHPHFOP_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPHFOP_EN_VAUDP18_SFT);
	/* Set input diff pair bias to min (20uA)*/
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
		RG_AUDHPDIFFINPBIASADJ_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPDIFFINPBIASADJ_VAUDP18_SFT);


	/* Enable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTAUXPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTAUXPWRUP_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLOUTAUXPWRUP_VAUDP18_SFT);
	/* Enable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLAUXFBRSW_EN_VAUDP18_MASK_SFT |
		RG_HPRAUXFBRSW_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLAUXFBRSW_EN_VAUDP18_SFT);
	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_AUDHPLAUXCM_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRAUXCM_EN_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLAUXCM_EN_VAUDP18_SFT);
	/* Enable HP driver bias circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPLPWRUP_IBIAS_VAUDP18_MASK_SFT |
		RG_AUDHPRPWRUP_IBIAS_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLPWRUP_IBIAS_VAUDP18_SFT);
	/* Enable HP driver core circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPLPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPRPWRUP_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLPWRUP_VAUDP18_SFT);
	/* Short HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLSHORT2HPLAUX_EN_VAUDP18_MASK_SFT |
		RG_HPRSHORT2HPRAUX_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLSHORT2HPLAUX_EN_VAUDP18_SFT);

	hp_in_pair_current(priv, true);

	usleep_range(100, 120);

	/* Enable HP main CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
		RG_DA_HPCMFB_EN_RCH_MASK_SFT,
		0x1 << RG_DA_HPCMFB_EN_RCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
		RG_DA_HPCMFB_EN_LCH_MASK_SFT,
		0x1 << RG_DA_HPCMFB_EN_LCH_SFT);
	/* Disable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_AUDHPLAUXCM_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRAUXCM_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLAUXCM_EN_VAUDP18_SFT);
	usleep_range(100, 120);

	/* Enable HP main output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTPWRUP_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLOUTPWRUP_VAUDP18_SFT);
	usleep_range(100, 120);

	/* Enable HPR/L main output stage step by step */
	hp_main_output_ramp(priv, true);

	/* Reduce HP aux feedback loop gain */
	hp_aux_feedback_loop_gain_ramp(priv, true);
	/* Disable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLAUXFBRSW_EN_VAUDP18_MASK_SFT |
		RG_HPRAUXFBRSW_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLAUXFBRSW_EN_VAUDP18_SFT);

	/* Disable HP mute */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPLMUTE_EN_VAUDP18_MASK_SFT |
		RG_HPRMUTE_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLMUTE_EN_VAUDP18_SFT);

	/* open HP output to AUX output */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLSHORT2HPLAUX_EN_VAUDP18_MASK_SFT |
		RG_HPRSHORT2HPRAUX_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLSHORT2HPLAUX_EN_VAUDP18_SFT);
	/* Disable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTAUXPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTAUXPWRUP_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLOUTAUXPWRUP_VAUDP18_SFT);
	/* Reset HPP/N STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);

	/* Disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true, false);

	/* Enable HFOP circuits for Hi-Fi mode */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_DAMP2ND_EN_VAUDP18_MASK_SFT,
		0x1 << RG_DAMP2ND_EN_VAUDP18_SFT);

	/* CMFB resistor with modulation Rwell levele */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLCMFB_RNWSEL_VAUDP18_MASK_SFT |
		RG_AUDHPRCMFB_RNWSEL_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLCMFB_RNWSEL_VAUDP18_SFT);
	/* Feedback resistor with modulation Rwell level */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLHPFB_RNWSEL_VAUDP18_MASK_SFT |
		RG_AUDHPRHPFB_RNWSEL_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLHPFB_RNWSEL_VAUDP18_SFT);

	/* Enable HP feedback SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
		RG_AUDHPHIFISWST_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDHPHIFISWST_EN_VAUDP18_SFT);

	/* Enable HD removed SW source-tie for Hi-Fi mode */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON10,
		RG_HPLHDRMSW_ST_EN_VAUDP18_MASK_SFT |
		RG_HPRHDRMSW_ST_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLHDRMSW_ST_EN_VAUDP18_SFT);

	/* Enable CMFB SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON12_H,
		DA_HPLCMFBSWST_EN_VAUDP18_MASK_SFT |
		DA_HPRCMFBSWST_EN_VAUDP18_MASK_SFT,
		0x3 << DA_HPLCMFBSWST_EN_VAUDP18_SFT);
	/* Enable HP input MUX SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_HPMUXST_EN_VAUDP18_MASK_SFT,
		0x1 << RG_HPMUXST_EN_VAUDP18_SFT);

	/* [7:6] HP 2nd order damp control
	 *  damp adjustimation automatically select by NLE
	 */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON49,
		0x3 << (RG_ABIDEC_RSVD1_VAUDP18_SFT + 6),
		0x3 << (RG_ABIDEC_RSVD1_VAUDP18_SFT + 6));
	/* Enable HPRL LN path feedback Rwell modulation */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLMAINCM2_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRMAINCM2_EN_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLMAINCM2_EN_VAUDP18_SFT);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON8,
		RG_AUDDACHP_HOLD_SW_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDDACHP_HOLD_SW_EN_VAUDP18_SFT);
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON3, 0xff);
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON3, 0x9f);
	/* Set DAC as 512uA mode */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x0);

	/* Enable Audio DAC  */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
		RG_AUDDACL_PWRUP_VAUDP18_MASK_SFT |
		RG_AUDDACR_PWRUP_VAUDP18_MASK_SFT,
		0x3 << RG_AUDDACL_PWRUP_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
		RG_AUDDACL_BIAS_PWRUP_VA32_MASK_SFT |
		RG_AUDDACR_BIAS_PWRUP_VA32_MASK_SFT,
		0x3 << RG_AUDDACL_BIAS_PWRUP_VA32_SFT);

	usleep_range(100, 120);

	/* Select to AVDD30_AUD  */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON43, 0x1);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON1,
		RG_AUDDAC_L_RPWM_EN_VAUDP18_MASK_SFT |
		RG_AUDDAC_R_RPWM_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDDAC_L_RPWM_EN_VAUDP18_SFT);

	/* Enable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON1,
		RG_AUDDAC_IREF_LN_SEL_VA32_MASK_SFT,
		0x3 << RG_AUDDAC_IREF_LN_SEL_VA32_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON1,
		RG_AUDDAC_IREF_LN_EN_VA32_MASK_SFT,
		0x1 << RG_AUDDAC_IREF_LN_EN_VA32_SFT);
	/* Enable low-noise mode of DAC */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON2,
		RG_AUDDAC_ILOCAL_LN_SEL_VA32_MASK_SFT,
		0x3 << RG_AUDDAC_ILOCAL_LN_SEL_VA32_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON2,
		RG_AUDDAC_ILOCAL_LN_EN_VA32_MASK_SFT,
		0x1 << RG_AUDDAC_ILOCAL_LN_EN_VA32_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON2,
		RG_AUDDAC_OPAMP_LN_EN_VA32_MASK_SFT,
		0x1 << RG_AUDDAC_OPAMP_LN_EN_VA32_SFT);

	/* Switch HPL MUX to audio DAC */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPLMUXINPUTSEL_VAUDP18_MASK_SFT,
		HP_MUX_HP << RG_AUDHPLMUXINPUTSEL_VAUDP18_SFT);
	/* Switch HPR MUX to audio DAC */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPRMUXINPUTSEL_VAUDP18_MASK_SFT,
		HP_MUX_HP << RG_AUDHPRMUXINPUTSEL_VAUDP18_SFT);
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x1);

	dev_info(priv->dev, "%s(), --\n", __func__);
}

static void stop_trim_hardware(struct mt6338_priv *priv)
{
	dev_info(priv->dev, "%s(), ++\n", __func__);
	/* HPR/HPL mux to open */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPLMUXINPUTSEL_VAUDP18_MASK_SFT,
		HP_MUX_OPEN << RG_AUDHPLMUXINPUTSEL_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPRMUXINPUTSEL_VAUDP18_MASK_SFT,
		HP_MUX_OPEN << RG_AUDHPRMUXINPUTSEL_VAUDP18_SFT);

	/* Disable low-noise mode of DAC */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON1, 0x0);
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON2, 0x0);

	/* Scrambler/RPMW selection */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON1,
		RG_AUDDAC_R_RPWM_EN_VAUDP18_MASK_SFT |
		RG_AUDDAC_L_RPWM_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDDAC_L_RPWM_EN_VAUDP18_SFT);

	/* Select to AVDD30_AUD  */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON43, 0x0);

	/* Disable Audio DAC */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
		RG_AUDDACL_PWRUP_VAUDP18_MASK_SFT |
		RG_AUDDACR_PWRUP_VAUDP18_MASK_SFT,
		0x0 << RG_AUDDACL_PWRUP_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON0,
		RG_AUDDACL_BIAS_PWRUP_VA32_MASK_SFT |
		RG_AUDDACR_BIAS_PWRUP_VA32_MASK_SFT,
		0x0 << RG_AUDDACL_BIAS_PWRUP_VA32_SFT);
	/* CMFB resistor with modulation Rwell levele */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLCMFB_RNWSEL_VAUDP18_MASK_SFT |
		RG_AUDHPRCMFB_RNWSEL_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLCMFB_RNWSEL_VAUDP18_SFT);
	/* Feedback resistor with modulation Rwell level */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_AUDHPLHPFB_RNWSEL_VAUDP18_MASK_SFT |
		RG_AUDHPRHPFB_RNWSEL_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLHPFB_RNWSEL_VAUDP18_SFT);
	/* Enable HP feedback SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
		RG_AUDHPHIFISWST_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPRHPFB_RNWSEL_VAUDP18_SFT);
	/* Enable HD removed SW source-tie for Hi-Fi mode */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON10,
		RG_HPLHDRMSW_ST_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLHDRMSW_ST_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON10,
		RG_HPRHDRMSW_ST_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPRHDRMSW_ST_EN_VAUDP18_SFT);
	/* Enable CMFB SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON12_H,
		DA_HPLCMFBSWST_EN_VAUDP18_MASK_SFT |
		DA_HPRCMFBSWST_EN_VAUDP18_MASK_SFT,
		0x0 << DA_HPLCMFBSWST_EN_VAUDP18_SFT);
	/* Enable HP input MUX SW source-tie */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_HPMUXST_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPMUXST_EN_VAUDP18_SFT);

	/* Enable 2nd order damp circuit for Hi-Fi mode*/
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_DAMP2ND_EN_VAUDP18_MASK_SFT,
		0x0 << RG_DAMP2ND_EN_VAUDP18_SFT);

	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true, true);

	/* Reset HPP/N STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x7 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x7 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);
	/* Short HP main output to HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTAUXPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTAUXPWRUP_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLOUTAUXPWRUP_VAUDP18_SFT);
	/* Enable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLSHORT2HPLAUX_EN_VAUDP18_MASK_SFT |
		RG_HPRSHORT2HPRAUX_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLSHORT2HPLAUX_EN_VAUDP18_SFT);

	/* decrease HPL/R gain to normal gain step by step */
	headset_volume_ramp(priv,
				priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL],
				HP_GAIN_0DB);

	/* Disable HP mute */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPLMUTE_EN_VAUDP18_MASK_SFT |
		RG_HPRMUTE_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLMUTE_EN_VAUDP18_SFT);

	/* Disable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLAUXFBRSW_EN_VAUDP18_MASK_SFT |
		RG_HPRAUXFBRSW_EN_VAUDP18_MASK_SFT,
		0x3 << RG_HPLAUXFBRSW_EN_VAUDP18_SFT);
	/* Reduce HP aux feedback loop gain */
	hp_aux_feedback_loop_gain_ramp(priv, false);

	/* decrease HPR/L main output stage step by step */
	hp_main_output_ramp(priv, false);

	/* Disable HP main output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTPWRUP_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLOUTPWRUP_VAUDP18_SFT);

	/* Enable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_AUDHPLAUXCM_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRAUXCM_EN_VAUDP18_MASK_SFT,
		0x3 << RG_AUDHPLAUXCM_EN_VAUDP18_SFT);

	/* Disable HP main CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
		RG_DA_HPCMFB_EN_RCH_MASK_SFT,
		0x0 << RG_DA_HPCMFB_EN_RCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
		RG_DA_HPCMFB_EN_LCH_MASK_SFT,
		0x0 << RG_DA_HPCMFB_EN_LCH_SFT);
	/* Decrease HP input pair current to 2'b00 step by step */
	hp_in_pair_current(priv, false);
	/* open HP output to AUX output */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLSHORT2HPLAUX_EN_VAUDP18_MASK_SFT |
		RG_HPRSHORT2HPRAUX_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLSHORT2HPLAUX_EN_VAUDP18_SFT);
	/* Disable HP driver core circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPLPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPRPWRUP_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLPWRUP_VAUDP18_SFT);
	/* Disable HP driver bias circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON4,
		RG_AUDHPLPWRUP_IBIAS_VAUDP18_MASK_SFT |
		RG_AUDHPRPWRUP_IBIAS_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLPWRUP_IBIAS_VAUDP18_SFT);
	/* Disable HP aux output stage */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON5,
		RG_AUDHPLOUTAUXPWRUP_VAUDP18_MASK_SFT |
		RG_AUDHPROUTAUXPWRUP_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLOUTAUXPWRUP_VAUDP18_SFT);

	/* Disable HP aux feedback loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLAUXFBRSW_EN_VAUDP18_MASK_SFT |
		RG_HPRAUXFBRSW_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLAUXFBRSW_EN_VAUDP18_SFT);

	/* Disable HP aux CMFB loop */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_AUDHPLAUXCM_EN_VAUDP18_MASK_SFT |
		RG_AUDHPRAUXCM_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLAUXCM_EN_VAUDP18_SFT);
	/* Disable HD removed SW when turn on sequence */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON9,
		RG_HPLHDRM_PFL_EN_VAUDP18_MASK_SFT |
		RG_HPRHDRM_PFL_EN_VAUDP18_MASK_SFT,
		0x0 << RG_HPLHDRM_PFL_EN_VAUDP18_SFT);
	/* Disable Neg R when turn on sequence */
	regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON12_H,
		DA_AUDHPLNEGR_EN_VAUDP18_MASK_SFT |
		DA_AUDHPRNEGR_EN_VAUDP18_MASK_SFT,
		0x0 << DA_AUDHPLNEGR_EN_VAUDP18_SFT);
	/* Set HP NREG segmentation */
	regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON12, 0x0);
	/* Enable HFOP circuits for Hi-Fi mode */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
		RG_AUDHPHFOP_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPHFOP_EN_VAUDP18_SFT);
	/* Enable HP damping ckt.  */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON15,
		RG_AUDHPDAMP_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPDAMP_EN_VAUDP18_SFT);
	/* Damping adjustment select. (Hi-Fi mode) */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON14,
		RG_AUDHPDAMP_ADJ_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPDAMP_ADJ_VAUDP18_SFT);
	/*Enable HPR/L main output stage to min*/
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M,
		RG_DA_HP_OUTSTG_RCH_MASK_SFT,
		0x0 << RG_DA_HP_OUTSTG_RCH_SFT);
	regmap_update_bits(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG,
		RG_DA_HP_OUTSTG_LCH_MASK_SFT,
		0x0 << RG_DA_HP_OUTSTG_LCH_SFT);

	/* Enable HP De-CMgain circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON16,
		RG_AUDHPMAINCM_COMP_EN_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPMAINCM_COMP_EN_VAUDP18_SFT);

	/* Set HPP/N STB enhance circuits */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON7,
		RG_HPROUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPROUTPUTSTBENH_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON6,
		RG_HPLOUTPUTSTBENH_VAUDP18_MASK_SFT,
		0x3 << RG_HPLOUTPUTSTBENH_VAUDP18_SFT);
	/* BAIS */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON31,
		RG_AUDIBIASPWRDN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDIBIASPWRDN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON30,
		RG_AUDBIASADJ_0_HP_VAUDP18_MASK_SFT,
		DRBIAS_6UA << RG_AUDBIASADJ_0_HP_VAUDP18_SFT);
	/* Disable Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(priv, true, false);

	/* Set HP damp parameter MSB & enable HP FB */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON3, 0xe3);
	/* Enable for NVREG */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON41,
		RG_NVREG_DACSW_L_EN_VAUDP18_MASK_SFT |
		RG_NVREG_DACSW_R_EN_VAUDP18_MASK_SFT,
		0x0 << RG_NVREG_DACSW_L_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON41,
		RG_NVREG_LC_BUF_L_EN_VAUDP18_MASK_SFT |
		RG_NVREG_LC_BUF_R_EN_VAUDP18_MASK_SFT,
		0x0 << RG_NVREG_LC_BUF_L_EN_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON41,
		RG_NVREG_HC_BUF_L_EN_VAUDP18_MASK_SFT |
		RG_NVREG_HC_BUF_R_EN_VAUDP18_MASK_SFT,
		0x0 << RG_NVREG_HC_BUF_L_EN_VAUDP18_SFT);
	ldo_select_to_min(priv, false, true);

	/* Disable NCP */
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG0,	0x0);

	/* Increase ESD resistance of AU_REFN */
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON10,
		RG_AUDREFN_DERES_EN_VAUDP18_MASK_SFT,
		0x1 << RG_AUDREFN_DERES_EN_VAUDP18_SFT);

	/* Disable AUDGLB */
	mt6338_set_aud_global_bias(priv, false);

	/* Disable DC Comp */
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_L,
		MT6338_AFE_DL_AUD_DC_COMP_EN_MASK_SFT,
		0x0 << MT6338_AFE_DL_AUD_DC_COMP_EN_SFT);

	/* R_AUD_DAC_MONO_SEL */
	regmap_update_bits(priv->regmap, MT6338_AFUNC_AUD_CON6_L,
		R_AUD_DAC_MONO_SEL_MASK_SFT,
		0x0 << R_AUD_DAC_MONO_SEL_SFT);

	/* turn off dl */
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0,
		AFE_DL_SRC_ON_TMP_CTL_PRE_MASK_SFT,
		0x0 << AFE_DL_SRC_ON_TMP_CTL_PRE_SFT);

	regmap_update_bits(priv->regmap, MT6338_AFE_TOP_CON0,
		AFE_ON_MASK_SFT,
		0x0 << AFE_ON_SFT);

	/* sdm fifo disable */

	/* disable scrambler output */
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON0_L, 0x20);
	/* disable scrambler output clock */
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON0_H, 0xca);
	/* sdm audio fifo clock power off */
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON2_L, 0x1);

	/* Disable AUD_CLK */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON46, 0x0);

	/* SRAM  power down */
	regmap_write(priv->regmap, MT6338_AUD_TOP_SRAM_CON, 0x6);

	regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H,
		RG_AUD208M_CK_PDN_MASK_SFT,
		0x1 << RG_AUD208M_CK_PDN_SFT);
	mt6338_set_dl_src(priv, false);

	/* Turn off AUDNCP_CLKDIV engine clock */
	mt6338_set_topck(priv, false);

	mt6338_set_pmu(priv, false);

	regmap_update_bits(priv->regmap, MT6338_VOWPLL_PMU_CON0,
		RG_VOWPLL_EN_MASK_SFT,
		0x0 << RG_VOWPLL_EN_SFT);

	/* Disable CLKSQ */
	mt6338_set_clksq(priv, false);

	regmap_update_bits(priv->regmap, MT6338_TOP_CKTST_CON0,
		RG_VCORE_26M_CK_TST_DIS_MASK_SFT,
		0x0 << RG_VCORE_26M_CK_TST_DIS_SFT);
	regmap_update_bits(priv->regmap, MT6338_TOP_CKTST_CON0,
		RG_DSPPLL_208M_CK_TST_DIS_MASK_SFT,
		0x0 << RG_DSPPLL_208M_CK_TST_DIS_SFT);

	/* XO_AUDIO_EN_M Disable */
	mt6338_set_dcxo(priv, false);
	mt6338_mtkaif_tx_disable(priv);
	/* Reset playback gpio (mosi/clk/sync) */
	mt6338_reset_playback_gpio(priv);

	dev_info(priv->dev, "%s(), --\n", __func__);
}

static int calculate_trim_result(struct mt6338_priv *priv, int *on_value, int *off_value,
				 int trimTime, int discard_num, int useful_num)
{
	int i, j, tmp, offset;

	/* sort */
	for (i = 0; i < trimTime - 1; i++) {
		for (j = 0; j < trimTime - 1 - i; j++) {
			if (on_value[j] > on_value[j + 1]) {
				tmp = on_value[j + 1];
				on_value[j + 1] = on_value[j];
				on_value[j] = tmp;
			}
			if (off_value[j] > off_value[j + 1]) {
				tmp = off_value[j + 1];
				off_value[j + 1] = off_value[j];
				off_value[j] = tmp;
			}
		}
	}
	/* calculate result */
	offset = 0;
	for (i = discard_num; i < trimTime - discard_num; i++)
		offset += on_value[i] - off_value[i];

	return DIV_ROUND_CLOSEST(offset, useful_num);
}

static void get_hp_dctrim_offset(struct mt6338_priv *priv,
				 int *hpl_trim, int *hpr_trim)
{
	int on_valueL[TRIM_TIMES], on_valueR[TRIM_TIMES];
	int off_valueL[TRIM_TIMES], off_valueR[TRIM_TIMES];
	int i;

	usleep_range(10 * 1000, 15 * 1000);
	regmap_update_bits(priv->regmap, MT6338_AUXADC_AVG_CON4,
			   0x7, AUXADC_AVG_256);

	/* set ana_gain as 0DB */
	priv->ana_gain[AUDIO_ANALOG_VOLUME_HPOUTL] = HP_GAIN_0DB;

	/* turn on trim buffer */
	start_trim_hardware(priv);

	/* l-channel */
	/* enable trim buffer */
	/* trimming buffer gain selection 18db */
	/* trimming buffer mux selection : HPL */
	prepare_enable_trim_buf(priv, true, TRIM_BUF_GAIN_18DB,
							TRIM_BUF_MUX_HPL);

	/* get buffer on auxadc value  */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		on_valueL[i] = mt6338_get_hpofs_auxadc(priv);

	/* trimming buffer mux selection : AU_REFN */
	set_trim_buf_in_mux(priv, TRIM_BUF_MUX_AU_REFN);

	/* get buffer off auxadc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		off_valueL[i] = mt6338_get_hpofs_auxadc(priv);

	/* r-channel */
	/* trimming buffer mux selection : HPR */
	set_trim_buf_in_mux(priv, TRIM_BUF_MUX_HPR);

	/* get buffer on auxadc value  */
	dev_dbg(priv->dev, "%s(), get on_valueR\n", __func__);
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		on_valueR[i] = mt6338_get_hpofs_auxadc(priv);

	/* trimming buffer mux selection : AU_REFN */
	set_trim_buf_in_mux(priv, TRIM_BUF_MUX_AU_REFN);

	/* get buffer off auxadc value	*/
	dev_dbg(priv->dev, "%s(), get off_valueR\n", __func__);
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		off_valueR[i] = mt6338_get_hpofs_auxadc(priv);
	/* disable trim buffer */
	/* reset trimming buffer mux to OPEN */
	/* reset trimming buffer gain selection 0db*/
	prepare_enable_trim_buf(priv, false, TRIM_BUF_GAIN_0DB,
							TRIM_BUF_MUX_OPEN);

	/* turn off trim buffer */
	stop_trim_hardware(priv);

	*hpl_trim = calculate_trim_result(priv, on_valueL, off_valueL,
					  TRIM_TIMES, TRIM_DISCARD_NUM,
					  TRIM_USEFUL_NUM);
	*hpr_trim = calculate_trim_result(priv, on_valueR, off_valueR,
					  TRIM_TIMES, TRIM_DISCARD_NUM,
					  TRIM_USEFUL_NUM);

	dev_info(priv->dev, "%s(), L_offset = %d, R_offset = %d\n",
		 __func__, *hpl_trim, *hpr_trim);
}

static void update_finetrim_offset(struct mt6338_priv *priv,
				   int step,
				   const unsigned int finetrim_code_l,
				   const unsigned int finetrim_code_r,
				   int *finetrim_offset_l,
				   int *finetrim_offset_r)
{
	int hpl_offset = 0, hpr_offset = 0;

	dev_dbg(priv->dev, "%s(), step%d finetrim_code(L/R) = (0x%x/0x%x)\n",
		__func__, step, finetrim_code_l, finetrim_code_r);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON12,
		RG_AUDHPLFINETRIM_VAUDP18_MASK_SFT,
		finetrim_code_l << RG_AUDHPLFINETRIM_VAUDP18_SFT);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON13,
		RG_AUDHPRFINETRIM_VAUDP18_MASK_SFT,
		finetrim_code_r << RG_AUDHPRFINETRIM_VAUDP18_SFT);

	get_hp_dctrim_offset(priv, &hpl_offset, &hpr_offset);
	*finetrim_offset_l = hpl_offset;
	*finetrim_offset_r = hpr_offset;

	dev_info(priv->dev, "%s(), step%d finetrim (code/value)(L/R) = (0x%x/%d)(0x%x/%d)\n",
		__func__, step, finetrim_code_l, *finetrim_offset_l,
		finetrim_code_r, *finetrim_offset_r);
}

static void update_trim_offset(struct mt6338_priv *priv,
			       int step,
			       const unsigned int trim_code_l,
			       const unsigned int trim_code_r,
			       int *trim_offset_l,
			       int *trim_offset_r)
{
	int hpl_offset = 0, hpr_offset = 0;

	dev_dbg(priv->dev, "%s(), step%d trim_code(L/R) = (0x%x/0x%x)\n",
		__func__, step, trim_code_l, trim_code_r);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON12,
		RG_AUDHPLTRIM_VAUDP18_MASK_SFT,
		trim_code_l << RG_AUDHPLTRIM_VAUDP18_SFT);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON13,
		RG_AUDHPRTRIM_VAUDP18_MASK_SFT,
		trim_code_r << RG_AUDHPRTRIM_VAUDP18_SFT);

	get_hp_dctrim_offset(priv, &hpl_offset, &hpr_offset);

	*trim_offset_l = hpl_offset;
	*trim_offset_r = hpr_offset;

	dev_info(priv->dev, "%s(), step%d trim (code/value)(L/R) = (0x%x/%d)(0x%x/%d)\n",
		__func__, step, trim_code_l, *trim_offset_l,
		trim_code_r, *trim_offset_r);
}

static unsigned int update_finetrim_code(const unsigned int trim_offset0,
					 const unsigned int trim_offset1,
					 const unsigned int trim_offset2)
{
	unsigned int ret_finetrim_code = 0;

	/* Base on finetrim[0/1/2], choose minimim finetrim_code */
	if (trim_offset0 < trim_offset1) {
		if (trim_offset0 < trim_offset2)
			ret_finetrim_code = 0;
		else /* (trim_offset0 >= trim_offset2) */
			ret_finetrim_code = 2;
	} else { /* (trim_offset0 >= trim_offset1) */
		if (trim_offset1 < trim_offset2)
			ret_finetrim_code = 1;
		else /* (trim_offset1 >= trim_offset2) */
			ret_finetrim_code = 2;
	}

	return ret_finetrim_code;
}

static unsigned int update_trim_code(const int *trim_value,
				     const unsigned int *trim_code)
{
	unsigned int ret_trim_code = 0;
	int ret_trim_val = 0;
	int i = 0;

	/* choose result */
	ret_trim_val = abs(trim_value[0]);
	for (i = 1; i < TRIM_STEP_NUM; i++) {
		if (abs(trim_value[i]) <= ret_trim_val) {
			ret_trim_code = i;
			ret_trim_val = abs(trim_value[i]);
		}
	}
	return trim_code[ret_trim_code];
}

static unsigned int refine_trim_code(const bool is_negative,
				     const int trim_value,
				     const unsigned int trim_code)
{
	unsigned int ret_trim_code;

	if (is_negative) { /* value<0, code+1; value>=0, code-1; */
		if (trim_code == 0x0 && trim_value >= 0)
			ret_trim_code = 0x11;
		else if (trim_code == 0xF && trim_value < 0)
			ret_trim_code = 0x0F;
		else
			ret_trim_code = trim_code - (trim_value < 0 ? (-1) : 1);
	} else { /* value<0, code-1; value>=0, code+1; */
		if (trim_code == 0x10 && trim_value < 0)
			ret_trim_code = 0x01;
		else if (trim_code == 0x1F && trim_value >= 0)
			ret_trim_code = 0x1F;
		else
			ret_trim_code = trim_code + (trim_value < 0 ? (-1) : 1);
	}
	return ret_trim_code;
}

static void calculate_lr_finetrim_code(struct mt6338_priv *priv)
{
	struct hp_trim_data *hp_trim = &priv->hp_trim_3_pole;

	int finetrim_l[TRIM_STEP_NUM - 1] = {0, 0, 0};
	int finetrim_r[TRIM_STEP_NUM - 1] = {0, 0, 0};
	unsigned int finetrim_l_code[TRIM_STEP_NUM - 1] = {0, 0, 0};
	unsigned int finetrim_r_code[TRIM_STEP_NUM - 1] = {0, 0, 0};
	unsigned int hpl_finetrim_code = 0, hpr_finetrim_code = 0;
	unsigned int step = 0;

	/* step0 */
	finetrim_l_code[0] = 0x0;
	finetrim_r_code[0] = 0x0;

	update_finetrim_offset(priv, 0,
			       finetrim_l_code[0], finetrim_r_code[0],
			       &finetrim_l[0], &finetrim_r[0]);
	dev_dbg(priv->dev, "%s(), step0 finetrim(R/L) = (%d/%d)\n",
		 __func__, finetrim_r[0], finetrim_l[0]);

	/* step1 */
	if (finetrim_l[0] < 0)
		finetrim_l_code[1] = 0x2;
	else /* (finetrim_l[0] >= 0) */
		finetrim_l_code[1] = 0x6;

	if (finetrim_r[0] < 0)
		finetrim_r_code[1] = 0x2;
	else /* (finetrim_r[0] >= 0) */
		finetrim_r_code[1] = 0x6;

	update_finetrim_offset(priv, 1,
			       finetrim_l_code[1], finetrim_r_code[1],
			       &finetrim_l[1], &finetrim_r[1]);
	dev_dbg(priv->dev, "%s(), step1 finetrim(R/L) = (%d/%d)\n",
		 __func__, finetrim_r[1], finetrim_l[1]);

	/* step2 */
	if (finetrim_l[0] < 0 && finetrim_l[1] < 0)
		finetrim_l_code[2] = 0x3;
	else if (finetrim_l[0] < 0 && finetrim_l[1] >= 0)
		finetrim_l_code[2] = 0x1;
	else if (finetrim_l[0] >= 0 && finetrim_l[1] < 0)
		finetrim_l_code[2] = 0x7;
	else /* (finetrim_l[0] >= 0 && finetrim_l[1] >= 0) */
		finetrim_l_code[2] = 0x5;

	if (finetrim_r[0] < 0 && finetrim_r[1] < 0)
		finetrim_r_code[2] = 0x3;
	else if (finetrim_r[0] < 0 && finetrim_r[1] >= 0)
		finetrim_r_code[2] = 0x1;
	else if (finetrim_r[0] >= 0 && finetrim_r[1] < 0)
		finetrim_r_code[2] = 0x7;
	else /* (finetrim_r[0] >= 0 && finetrim_r[1] >= 0) */
		finetrim_r_code[2] = 0x5;

	update_finetrim_offset(priv, 2,
			       finetrim_l_code[2], finetrim_r_code[2],
			       &finetrim_l[2], &finetrim_r[2]);
	dev_dbg(priv->dev, "%s(), step2 finetrim(R/L) = (%d/%d)\n",
		 __func__, finetrim_r[2], finetrim_l[2]);

	step = update_finetrim_code(finetrim_l[0],
				    finetrim_l[1],
				    finetrim_l[2]);
	hpl_finetrim_code = finetrim_l_code[step];

	step = update_finetrim_code(finetrim_r[0],
				    finetrim_r[1],
				    finetrim_r[2]);
	hpr_finetrim_code = finetrim_r_code[step];

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON12,
		RG_AUDHPLFINETRIM_VAUDP18_MASK_SFT,
		hpl_finetrim_code << RG_AUDHPLFINETRIM_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON13,
		RG_AUDHPRFINETRIM_VAUDP18_MASK_SFT,
		hpr_finetrim_code << RG_AUDHPRFINETRIM_VAUDP18_SFT);

	hp_trim->hp_fine_trim_l = hpl_finetrim_code;
	hp_trim->hp_fine_trim_r = hpr_finetrim_code;

	dev_info(priv->dev, "%s(), result finetrim_code(R/L) = (0x%x/0x%x)\n",
		 __func__, hpr_finetrim_code, hpl_finetrim_code);
}

static void calculate_lr_trim_code(struct mt6338_priv *priv)
{
	struct hp_trim_data *hp_trim_3_pole = &priv->hp_trim_3_pole;
	struct hp_trim_data *hp_trim_4_pole = &priv->hp_trim_4_pole;

	int trim_l[TRIM_STEP_NUM] = {0, 0, 0, 0};
	int trim_r[TRIM_STEP_NUM] = {0, 0, 0, 0};
	unsigned int trim_l_code[TRIM_STEP_NUM] = {0, 0, 0, 0};
	unsigned int trim_r_code[TRIM_STEP_NUM] = {0, 0, 0, 0};

	unsigned int hpl_trim_code, hpr_trim_code;
	bool hpl_negative, hpr_negative;

	dev_dbg(priv->dev, "%s(), Start DCtrim Calibrating\n", __func__);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON12,
		RG_AUDHPLFINETRIM_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPLFINETRIM_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON13,
		RG_AUDHPRFINETRIM_VAUDP18_MASK_SFT,
		0x0 << RG_AUDHPRFINETRIM_VAUDP18_SFT);


	/* Start step0, set trim code to 0x0 */
	trim_l_code[0] = 0x0;
	trim_r_code[0] = 0x0;

	update_trim_offset(priv, 0, trim_l_code[0], trim_r_code[0],
			   &trim_l[0], &trim_r[0]);
	dev_dbg(priv->dev, "%s(), step0 trim (code/value)(L/R) = (0x%x/%d)(0x%x/%d)\n",
		 __func__, trim_l_code[0], trim_l[0],
		 trim_r_code[0], trim_r[0]);

	if (trim_l[0] == 0 && trim_r[0] == 0) {
		hpl_trim_code = trim_l_code[0];
		hpr_trim_code = trim_r_code[0];
		goto EXIT;
	}

	/* start step1, set trim code to 0x2 or 0x12 */
	if (trim_l[0] < 0) {
		hpl_negative = true;
		trim_l_code[1] = 0x2;
	} else { /* (trim_l[0] >= 0) */
		hpl_negative = false;
		trim_l_code[0] = 0x10;
		trim_l_code[1] = 0x12;
	}
	if (trim_r[0] < 0) {
		hpr_negative = true;
		trim_r_code[1] = 0x2;
	} else { /* (trim_r[0] >= 0) */
		hpr_negative = false;
		trim_r_code[0] = 0x10;
		trim_r_code[1] = 0x12;
	}

	update_trim_offset(priv, 1, trim_l_code[1], trim_r_code[1],
			   &trim_l[1], &trim_r[1]);
	dev_dbg(priv->dev, "%s(), step1 trim (code/value)(L/R) = (0x%x/%d)(0x%x/%d)\n",
		 __func__, trim_l_code[1], trim_l[1],
		 trim_r_code[1], trim_r[1]);

	if (trim_l[1] == 0 && trim_r[1] == 0) {
		hpl_trim_code = trim_l_code[1];
		hpr_trim_code = trim_r_code[1];
		goto EXIT;
	}

	/* prevent divid to 0 */
	if ((trim_l[0] == trim_l[1]) ||
		(trim_r[0] == trim_r[1])) {
		hpl_trim_code = trim_l_code[1];
		hpr_trim_code = trim_r_code[1];
		goto EXIT;
	}

	/* start step2, calculate approximate solution*/
	/* l-channel, find trim offset per trim code step */
	trim_l_code[2] = (((abs(trim_l[0]) * 3) /
			    abs(trim_l[0] - trim_l[1]))	+ 1) / 2;
	if (trim_l_code[2] > 0xF)
		trim_l_code[2] = 0xF;
	trim_l_code[2] = trim_l_code[2] + (trim_l[0] > 0 ? 16 : 0);

	if (trim_l_code[2] == 0x10)
		trim_l_code[0] = 0x10;

	/* r-channel, find trim offset per trim code step */
	trim_r_code[2] = (((abs(trim_r[0]) * 3) /
			    abs(trim_r[0] - trim_r[1])) + 1) / 2;
	if (trim_r_code[2] > 0xF)
		trim_r_code[2] = 0xF;
	trim_r_code[2] = trim_r_code[2] + (trim_r[0] > 0 ? 16 : 0);

	if (trim_r_code[2] == 0x10)
		trim_r_code[0] = 0x10;

	update_trim_offset(priv, 2,
			   trim_l_code[2], trim_r_code[2],
			   &trim_l[2], &trim_r[2]);
	dev_dbg(priv->dev, "%s(), step2 trim (code/value)(L/R) = (0x%x/%d)(0x%x/%d)\n",
		 __func__, trim_l_code[2], trim_l[2],
		 trim_r_code[2], trim_r[2]);

	if (trim_l[2] == 0 && trim_r[2] == 0) {
		hpl_trim_code = trim_l_code[2];
		hpr_trim_code = trim_r_code[2];
		goto EXIT;
	}

	/* start step3, lr-channel fine tune (+1 or -1) */
	trim_l_code[3] = refine_trim_code(hpl_negative,
					  trim_l[2], trim_l_code[2]);
	trim_r_code[3] = refine_trim_code(hpr_negative,
					  trim_r[2], trim_r_code[2]);

	dev_dbg(priv->dev, "%s(), step3 trim_code(L/R) = (0x%x/0x%x)\n",
		 __func__, trim_l_code[3], trim_r_code[3]);

	if ((trim_l_code[2] != 0x00 && trim_l_code[2] != 0x02 &&
	     trim_l_code[2] != 0x10 && trim_l_code[2] != 0x12) ||
	    (trim_r_code[2] != 0x00 && trim_r_code[2] != 0x02 &&
	     trim_r_code[2] != 0x10 && trim_r_code[2] != 0x12)) {
		dev_dbg(priv->dev, "%s(), need to calculate step4 trim_code\n",
			 __func__);

		update_trim_offset(priv, 3,
				   trim_l_code[3], trim_r_code[3],
				   &trim_l[3], &trim_r[3]);
		dev_dbg(priv->dev, "%s(), step3 trim (code/value)(L/R) = (0x%x/%d)(0x%x/%d)\n",
			 __func__, trim_l_code[3], trim_l[3],
			 trim_r_code[3], trim_r[3]);

		hpl_trim_code = update_trim_code(trim_l, trim_l_code);

		hpr_trim_code = update_trim_code(trim_r, trim_r_code);
	} else {
		hpl_trim_code = trim_l_code[3];
		hpr_trim_code = trim_r_code[3];
	}

EXIT:

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON12,
		RG_AUDHPLTRIM_VAUDP18_MASK_SFT,
		hpl_trim_code << RG_AUDHPLTRIM_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_PMU_CON13,
		RG_AUDHPRTRIM_VAUDP18_MASK_SFT,
		hpr_trim_code << RG_AUDHPRTRIM_VAUDP18_SFT);

	hp_trim_3_pole->hp_trim_l = hpl_trim_code;
	hp_trim_3_pole->hp_trim_r = hpr_trim_code;
	hp_trim_4_pole->hp_trim_l = hpl_trim_code;
	hp_trim_4_pole->hp_trim_r = hpr_trim_code;

	dev_info(priv->dev, "%s(), result hp_trim_code(L/R) = (0x%x/0x%x)\n",
		 __func__, hpl_trim_code, hpr_trim_code);
}
#endif /* #if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING) */

static void get_hp_trim_offset(struct mt6338_priv *priv, bool force)
{
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	struct dc_trim_data *dc_trim = &priv->dc_trim;
	struct hp_trim_data *hp_trim_3_pole = &priv->hp_trim_3_pole;

	if (dc_trim->calibrated && !force)
		return;

	dev_info(priv->dev, "%s(), Start Get DCtrim", __func__);
	enable_trim_circuit(priv, true);
	calculate_lr_trim_code(priv);
	calculate_lr_finetrim_code(priv);
	enable_trim_circuit(priv, false);

	dc_trim->calibrated = true;
	dev_info(priv->dev, "%s(), after trim_code R:(0x%x/0x%x), L:(0x%x/0x%x)",
		 __func__,
		 hp_trim_3_pole->hp_fine_trim_r, hp_trim_3_pole->hp_trim_r,
		 hp_trim_3_pole->hp_fine_trim_l, hp_trim_3_pole->hp_trim_l);
#else
	dev_info(priv->dev, "%s(), bypass while FPGA", __func__);
#endif
}

static int dc_trim_thread(void *arg)
{
	struct mt6338_priv *priv = arg;

	get_hp_trim_offset(priv, false);

#if IS_ENABLED(CONFIG_SND_SOC_MT6338_ACCDET)
	mt6338_accdet_late_init(0);
#endif
	do_exit(0);

	return 0;
}
/* Headphone Impedance Detection */
int mt6338_set_codec_ops(struct snd_soc_component *cmpnt,
			 struct mt6338_codec_ops *ops)
{
#ifdef USE_AP_DC
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	priv->ops.enable_dc_compensation = ops->enable_dc_compensation;
	priv->ops.set_lch_dc_compensation = ops->set_lch_dc_compensation;
	priv->ops.set_rch_dc_compensation = ops->set_rch_dc_compensation;
	priv->ops.adda_dl_gain_control = ops->adda_dl_gain_control;
#endif
	return 0;
}
EXPORT_SYMBOL(mt6338_set_codec_ops);

static struct bin_attribute codec_dev_attr_reg = {
	.attr = {
		.name = "mtk_audio_codec",
		.mode = 0600, /* permission */
	},
	.size = CODEC_SYS_DEBUG_SIZE,
	.read = mt6338_codec_sysfs_read,
	.write = mt6338_codec_sysfs_write,
};

static struct bin_attribute *mtk_codec_bin_attrs[] = {
	&codec_dev_attr_reg,
	NULL,
};

static struct attribute_group codec_bin_attr_group = {
	.name = "mtk_codec_attrs",
	.bin_attrs = mtk_codec_bin_attrs,
};


static int mtk_calculate_impedance_formula(int pcm_offset, int aux_diff)
{
	/* The formula is from DE programming guide */
	/* should be mantain by pmic owner */
	/* R = V /(I*16) */
	/* V = auxDiff * (1800mv /auxResolution)  /TrimBufGain */
	/* I =  pcmOffset * DAC_constant * Gsdm * Gibuf */

	long val = 3600000 / pcm_offset * aux_diff / 16;

	return (int)DIV_ROUND_CLOSEST(val, 7832);
}

static int calculate_impedance(struct mt6338_priv *priv,
			       int dc_init, int dc_input,
			       short pcm_offset,
			       const unsigned int detect_times)
{
	int dc_value;
	int r_tmp = 0;

	if (dc_input < dc_init) {
		dev_warn(priv->dev, "%s(), Wrong[%d] : dc_input(%d) < dc_init(%d)\n",
			 __func__, pcm_offset, dc_input, dc_init);
		return 0;
	}

	dc_value = dc_input - dc_init;
	r_tmp = mtk_calculate_impedance_formula(pcm_offset-1, dc_value);
	r_tmp = DIV_ROUND_CLOSEST(r_tmp, detect_times);

	/* Efuse calibration */
	if ((priv->hp_current_calibrate_val != 0) && (r_tmp != 0)) {
		dev_info(priv->dev, "%s(), Before Calibration from EFUSE: %d, R: %d\n",
			 __func__, priv->hp_current_calibrate_val, r_tmp);
		r_tmp = DIV_ROUND_CLOSEST(
				r_tmp * 128 + priv->hp_current_calibrate_val,
				128);
	}

	dev_dbg(priv->dev, "%s(), pcm_offset %d dcoffset %d detected resistor is %d\n",
		__func__, pcm_offset, dc_value, r_tmp);

	return r_tmp;
}

static int detect_impedance(struct mt6338_priv *priv)
{
	const unsigned int num_detect = 8;
	int i;
	int dc_sum = 0, detect_sum = 0;
	int pick_impedance = 0, impedance = 0, phase_flag = 0;
	int cur_dc = 0;
	unsigned int value = 0;

	/* params by chip */
	int auxcable_impedance = 5000;
	/* should little lower than auxadc max resolution */
	int auxadc_upper_bound = 32630;
	/* Dc ramp up and ramp down step */
	int dc_step = 96 / 16;
	/* Phase 0 : high impedance with worst resolution */
	int dc_phase0 = 288 / 16;
	/* Phase 1 : median impedance with normal resolution */
	int dc_phase1 = 1440 / 16;
	/* Phase 2 : low impedance with better resolution */
	int dc_phase2 = 4032 / 16;
	/* Resistance Threshold of phase 2 and phase 1 */
	int resistance_1st_threshold = 250;
	/* Resistance Threshold of phase 1 and phase 0 */
	int resistance_2nd_threshold = 1000;

	if (priv->ops.adda_dl_gain_control) {
		priv->ops.adda_dl_gain_control(true);
	} else {
		mt6338_adda_dl_gain_control(priv, true);
		dev_warn(priv->dev, "%s(), adda_dl_gain_control ops not ready\n",
			 __func__);
	}

	if (priv->ops.enable_dc_compensation &&
	    priv->ops.set_lch_dc_compensation &&
	    priv->ops.set_rch_dc_compensation) {
		priv->ops.set_lch_dc_compensation(0);
		priv->ops.set_rch_dc_compensation(0);
		priv->ops.enable_dc_compensation(true);
	} else {
		mt6338_set_lch_dc_compensation(priv, -1);
		mt6338_set_rch_dc_compensation(priv, -1);
		mt6338_enable_dc_compensation(priv, true);
		dev_warn(priv->dev, "%s(), dc compensation ops not ready\n",
			 __func__);
	}

	prepare_enable_trim_buf(priv, true, TRIM_BUF_GAIN_18DB,
							TRIM_BUF_MUX_HPR);
	/* AUXADC_AVG_NUM_HPC */
	regmap_update_bits(priv->regmap, MT6338_AUXADC_AVG_CON4,
		0x7, AUXADC_AVG_64);

	for (cur_dc = 0; cur_dc <= dc_phase2; cur_dc += dc_step) {
		/* apply dc by dc compensation: 16bit MSB and negative value */
		if (cur_dc != 0) {
			if (priv->ops.enable_dc_compensation &&
				priv->ops.set_lch_dc_compensation &&
				priv->ops.set_rch_dc_compensation) {
				priv->ops.set_lch_dc_compensation(-cur_dc << 16);
				priv->ops.set_rch_dc_compensation(-cur_dc << 16);
			} else {
				mt6338_set_lch_dc_compensation(priv, -cur_dc);
				mt6338_set_rch_dc_compensation(priv, -cur_dc);
			}
		}
		/* save for DC = 0 offset */
		if (cur_dc == 0) {
			usleep_range(1 * 1000, 1 * 1000);
			dc_sum = 0;
			for (i = 0; i < num_detect; i++)
				dc_sum += mt6338_get_hpofs_auxadc(priv);

			if ((dc_sum / num_detect) > auxadc_upper_bound) {
				dev_info(priv->dev, "%s(), cur_dc == 0, auxadc value %d > auxadc_upper_bound %d\n",
					 __func__,
					 dc_sum / num_detect,
					 auxadc_upper_bound);
				impedance = auxcable_impedance;
				break;
			}
		}

		/* start checking */
		if (cur_dc == dc_phase0) {
			usleep_range(1 * 1000, 1 * 1000);
			detect_sum = 0;
			detect_sum = mt6338_get_hpofs_auxadc(priv);

			if ((dc_sum / num_detect) == detect_sum) {
				dev_info(priv->dev, "%s(), dc_sum / num_detect %d == detect_sum %d\n",
					 __func__,
					 dc_sum / num_detect, detect_sum);
				impedance = auxcable_impedance;
				break;
			}

			pick_impedance = calculate_impedance(
						priv,
						dc_sum / num_detect,
						detect_sum, cur_dc, 1);

			if (pick_impedance < resistance_1st_threshold) {
				phase_flag = 2;
				continue;
			} else if (pick_impedance < resistance_2nd_threshold) {
				phase_flag = 1;
				continue;
			}

			/* Phase 0 : detect range 1kohm to 5kohm impedance */
			for (i = 1; i < num_detect; i++)
				detect_sum += mt6338_get_hpofs_auxadc(priv);

			/* if auxadc > 32630 , the hpImpedance is over 5k ohm */
			if ((detect_sum / num_detect) > auxadc_upper_bound)
				impedance = auxcable_impedance;
			else
				impedance = calculate_impedance(priv,
								dc_sum,
								detect_sum,
								cur_dc,
								num_detect);
			break;
		}

		/* Phase 1 : detect range 250ohm to 1000ohm impedance */
		if (phase_flag == 1 && cur_dc == dc_phase1) {
			usleep_range(1 * 1000, 1 * 1000);
			detect_sum = 0;
			for (i = 0; i < num_detect; i++)
				detect_sum += mt6338_get_hpofs_auxadc(priv);

			impedance = calculate_impedance(priv,
							dc_sum, detect_sum,
							cur_dc, num_detect);
			break;
		}

		/* Phase 2 : detect under 250ohm impedance */
		if (phase_flag == 2 && cur_dc == dc_phase2) {
			usleep_range(1 * 1000, 1 * 1000);
			detect_sum = 0;
			for (i = 0; i < num_detect; i++)
				detect_sum += mt6338_get_hpofs_auxadc(priv);

			impedance = calculate_impedance(priv,
							dc_sum, detect_sum,
							cur_dc, num_detect);
			break;
		}
		usleep_range(1 * 200, 1 * 200);
	}

	if (PARALLEL_OHM != 0) {
		if (impedance < PARALLEL_OHM) {
			impedance = DIV_ROUND_CLOSEST(impedance * PARALLEL_OHM,
						      PARALLEL_OHM - impedance);
		} else {
			dev_warn(priv->dev, "%s(), PARALLEL_OHM %d <= impedance %d\n",
				 __func__, PARALLEL_OHM, impedance);
		}
	}

	regmap_read(priv->regmap, MT6338_AUXADC_AVG_CON4, &value);
	dev_info(priv->dev,
		 "%s(), phase %d [dc,detect]Sum %d times [%d,%d], hp_impedance %d, pick_impedance %d, AUXADC_CON10 0x%x\n",
		 __func__, phase_flag, num_detect, dc_sum, detect_sum,
		 impedance, pick_impedance, value);

	/* Ramp-Down */
	while (cur_dc > dc_step) {
		cur_dc -= dc_step;
		/* apply dc by dc compensation: 16bit MSB and negative value */
		if (priv->ops.enable_dc_compensation &&
			priv->ops.set_lch_dc_compensation &&
			priv->ops.set_rch_dc_compensation) {
			priv->ops.set_lch_dc_compensation(-cur_dc << 16);
			priv->ops.set_rch_dc_compensation(-cur_dc << 16);
		} else {
			mt6338_set_lch_dc_compensation(priv, -cur_dc);
			mt6338_set_rch_dc_compensation(priv, -cur_dc);
		}
		usleep_range(1 * 200, 1 * 200);
	}
	if (priv->ops.enable_dc_compensation &&
	    priv->ops.set_lch_dc_compensation &&
	    priv->ops.set_rch_dc_compensation) {
		priv->ops.set_lch_dc_compensation(0);
		priv->ops.set_rch_dc_compensation(0);
		priv->ops.enable_dc_compensation(false);
		priv->ops.adda_dl_gain_control(false);
	} else {
		mt6338_enable_dc_compensation(priv, false);
		mt6338_adda_dl_gain_control(priv, false);
		mt6338_set_lch_dc_compensation(priv, 0);
		mt6338_set_rch_dc_compensation(priv, 0);
	}
	prepare_enable_trim_buf(priv, false, TRIM_BUF_GAIN_18DB,
							TRIM_BUF_MUX_OPEN);

	return impedance;
}

static int hp_impedance_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (priv->dev_counter[DEVICE_HP] <= 0 ||
	    priv->mux_select[MUX_HP_L] != HP_MUX_HP_IMPEDANCE) {
		dev_warn(priv->dev, "%s(), counter %d <= 0 || mux_select[MUX_HP_L] %d != HP_MUX_HP_IMPEDANCE\n",
			 __func__,
			 priv->dev_counter[DEVICE_HP],
			 priv->mux_select[MUX_HP_L]);
		ucontrol->value.integer.value[0] = priv->hp_impedance;
		return 0;
	}

	priv->hp_impedance = detect_impedance(priv);

	ucontrol->value.integer.value[0] = priv->hp_impedance;

	dev_info(priv->dev, "%s(), hp_impedance = %d, efuse = %d\n",
		 __func__, priv->hp_impedance, priv->hp_current_calibrate_val);

	return 0;
}

static int get_hp_current_calibrate_val(struct mt6338_priv *priv)
{
#if IS_ENABLED(CONFIG_MT6338_EFUSE)
	int ret = 0;
	unsigned short efuse_val = 0;
	int value, sign;

	/* set eFuse register index */
	/* HPDET_COMP[6:0] @ efuse bit 464 ~ 470 */
	/* HPDET_COMP_SIGN @ efuse bit 471 */
	/* 464 / 8 = 58(0x3a) bytes */
	ret = nvmem_device_read(priv->hp_efuse, 0x3a, 1, &efuse_val);
	if (ret < 0) {
		dev_err(priv->dev, "%s(), efuse read fail: %d\n", __func__,
			ret);
		efuse_val = 0;
	}

	/* extract value and signed from HPDET_COMP[6:0] & HPDET_COMP_SIGN */
	sign = (efuse_val >> 7) & 0x1;
	value = efuse_val & 0x7f;
	value = sign ? -value : value;

	dev_dbg(priv->dev, "%s(), efuse: %d\n", __func__, value);

	return value;
#else
	return 0;
#endif
}

static int set_idac_trim_val(struct mt6338_priv *priv)
{
#if IS_ENABLED(CONFIG_MT6338_EFUSE)
	int ret = 0;
	unsigned short efuse_val = 0;
	int value_l, value_r;

	/* set eFuse register index */
	/* HPL_POS_LARGE[6:0] @ efuse bit 789~795 */
	/* HPR_POS_LARGE[6:0] @ efuse bit 796~ 802 */
	/* 784 / 8 = 98(0x62) bytes */
	ret = nvmem_device_read(priv->hp_efuse, 0x62, 2, &efuse_val);
	value_l = (efuse_val >> 5) & 0x7f;
	ret = nvmem_device_read(priv->hp_efuse, 0x63, 2, &efuse_val);
	value_r = (efuse_val >> 4) & 0x7f;

	dev_info(priv->dev, "%s(), efuse_l: %d efuse_r: %d\n",
			 __func__, value_l, value_r);

	regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON0,
		RG_AUDDACHPL_TRIM_LARGE_VAUDP18_MASK_SFT,
		value_l << RG_AUDDACHPL_TRIM_LARGE_VAUDP18_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUDDEC_2_PMU_CON2,
		RG_AUDDACHPR_TRIM_LARGE_VAUDP18_MASK_SFT,
		value_r << RG_AUDDACHPR_TRIM_LARGE_VAUDP18_SFT);
	return true;
#else
	return 0;
#endif
}

/* vow control */
static void *get_vow_coeff_by_name(struct mt6338_priv *priv,
				   const char *name)
{
	if (strcmp(name, "Audio VOWCFG0 Data") == 0)
		return &(priv->reg_afe_vow_vad_cfg0);
	else if (strcmp(name, "Audio VOWCFG1 Data") == 0)
		return &(priv->reg_afe_vow_vad_cfg1);
	else if (strcmp(name, "Audio VOWCFG2 Data") == 0)
		return &(priv->reg_afe_vow_vad_cfg2);
	else if (strcmp(name, "Audio VOWCFG3 Data") == 0)
		return &(priv->reg_afe_vow_vad_cfg3);
	else if (strcmp(name, "Audio VOWCFG4 Data") == 0)
		return &(priv->reg_afe_vow_vad_cfg4);
	else if (strcmp(name, "Audio VOWCFG5 Data") == 0)
		return &(priv->reg_afe_vow_vad_cfg5);
	else if (strcmp(name, "Audio_VOW_Periodic") == 0)
		return &(priv->reg_afe_vow_periodic);
	else if (strcmp(name, "Audio_VOW_Periodic_Param") == 0)
		return (void *)&(priv->vow_periodic_param);
	else
		return NULL;
}

static int audio_vow_cfg_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int *vow_cfg;

	vow_cfg = (int *)get_vow_coeff_by_name(priv, kcontrol->id.name);
	if (!vow_cfg) {
		dev_err(priv->dev, "%s(), vow_cfg == NULL\n", __func__);
		return -EINVAL;
	}
	dev_info(priv->dev, "%s(), %s = 0x%x\n",
		 __func__, kcontrol->id.name, *vow_cfg);

	ucontrol->value.integer.value[0] = *vow_cfg;
	return 0;
}

static int audio_vow_cfg_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int index = ucontrol->value.integer.value[0];
	int *vow_cfg;

	vow_cfg = (int *)get_vow_coeff_by_name(priv, kcontrol->id.name);
	if (!vow_cfg) {
		dev_err(priv->dev, "%s(), vow_cfg == NULL\n", __func__);
		return -EINVAL;
	}
	dev_info(priv->dev, "%s(), %s = 0x%x\n",
		 __func__, kcontrol->id.name, index);

	*vow_cfg = index;
	return 0;
}

static int audio_vow_periodic_parm_set(struct snd_kcontrol *kcontrol,
				       const unsigned int __user *data,
				       unsigned int size)
{
	int ret = 0;
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	struct mt6338_vow_periodic_on_off_data *vow_param_cfg;

	dev_info(priv->dev, "%s(), size = %d\n", __func__, size);
	if (size > sizeof(struct mt6338_vow_periodic_on_off_data))
		return -EINVAL;
	vow_param_cfg = (struct mt6338_vow_periodic_on_off_data *)
	get_vow_coeff_by_name(priv, kcontrol->id.name);
	if (copy_from_user(vow_param_cfg, data,
			   sizeof(struct mt6338_vow_periodic_on_off_data))) {
		dev_info(priv->dev, "%s(),Fail copy to user Ptr:%p,r_sz:%zu\n",
			 __func__,
			 data,
			 sizeof(struct mt6338_vow_periodic_on_off_data));
		ret = -EFAULT;
	}
	return ret;
}

static const struct snd_kcontrol_new mt6338_snd_vow_controls[] = {
	SOC_SINGLE_EXT("Audio VOWCFG0 Data",
		SND_SOC_NOPM, 0, 0x80000, 0,
		audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG1 Data",
		SND_SOC_NOPM, 0, 0x80000, 0,
		audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG2 Data",
		SND_SOC_NOPM, 0, 0x80000, 0,
		audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG3 Data",
		SND_SOC_NOPM, 0, 0x80000, 0,
		audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG4 Data",
		SND_SOC_NOPM, 0, 0x80000, 0,
		audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio VOWCFG5 Data",
		SND_SOC_NOPM, 0, 0x80000, 0,
		audio_vow_cfg_get, audio_vow_cfg_set),
	SOC_SINGLE_EXT("Audio_VOW_Periodic",
		SND_SOC_NOPM, 0, 0x80000, 0,
		audio_vow_cfg_get, audio_vow_cfg_set),
	SND_SOC_BYTES_TLV("Audio_VOW_Periodic_Param",
		sizeof(struct mt6338_vow_periodic_on_off_data),
		NULL, audio_vow_periodic_parm_set),
};

/* misc control */
static const char *const off_on_function[] = {"Off", "On"};

static int hp_plugged_in_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	ucontrol->value.integer.value[0] = priv->hp_plugged;
	return 0;
}

static int hp_plugged_in_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(off_on_function)) {
		dev_warn(priv->dev, "%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	priv->hp_plugged = ucontrol->value.integer.value[0];

	return 0;
}

static const char *const hifi_on_function[] = {"Off", "On", "NLE"};
static int hp_hifi_mode_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	ucontrol->value.integer.value[0] = priv->hp_hifi_mode;
	return 0;
}

static int hp_hifi_mode_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(hifi_on_function)) {
		dev_info(priv->dev, "%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	priv->hp_hifi_mode = ucontrol->value.integer.value[0];

	return 0;
}

static int mic_hifi_mode_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	ucontrol->value.integer.value[0] = priv->mic_hifi_mode;
	return 0;
}

static int mic_hifi_mode_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(off_on_function)) {
		dev_info(priv->dev, "%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	priv->mic_hifi_mode = ucontrol->value.integer.value[0];

	return 0;
}

static int mic_ulcf_en_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	ucontrol->value.integer.value[0] = priv->mic_ulcf_en;
	return 0;
}

static int mic_ulcf_en_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(off_on_function)) {
		dev_info(priv->dev, "%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	priv->mic_ulcf_en = ucontrol->value.integer.value[0];

	return 0;
}

static const struct soc_enum misc_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(off_on_function), off_on_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(hifi_on_function), hifi_on_function),
};

static int mt6338_rcv_dcc_set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);

	/* 3:hwgain1/2 swap & bypass HWgain1/2 */
	regmap_write(priv->regmap, MT6338_AFE_TOP_DEBUG0, 0xc4);

	/* receiver downlink */
	mt6338_set_playback_gpio(priv);
	regmap_update_bits(priv->regmap, MT6338_LDO_VAUD18_CON0,
		RG_LDO_VAUD18_EN_0_MASK_SFT, 0x1);

	regmap_write(priv->regmap, MT6338_VPLL18_PMU_CON0, 0xc0);

	/* Enable/disable CLKSQ 26MHz */
	regmap_write(priv->regmap, MT6338_CLKSQ_PMU_CON0, 0xe);


	regmap_update_bits(priv->regmap, MT6338_VOWPLL_PMU_CON0,
		RG_VOWPLL_EN_MASK_SFT,
		0x1 << RG_VOWPLL_EN_SFT);
	/* PLL208M setting */
	regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON5, 0x2);
	regmap_write(priv->regmap, MT6338_PLL208M_PMU_CON4, 0x17);
	/* PLL208M setting */
	regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0,
		RG_AUDNCP_CK_PDN_MASK_SFT,
		0x0 << RG_AUDNCP_CK_PDN_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0,
		RG_ZCD13M_CK_PDN_MASK_SFT,
		0x0 << RG_ZCD13M_CK_PDN_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0,
		RG_AUDIF_CK_PDN_SFT,
		0x0 << RG_AUDIF_CK_PDN_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0,
		RG_AUDIF_CK_PDN_MASK_SFT,
		0x0 << RG_AUDIF_CK_PDN_SFT);
	regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H,
		RG_AUD208M_CK_PDN_MASK_SFT,
		0x0 << RG_AUD208M_CK_PDN_SFT);

	/* sram power on */
	regmap_write(priv->regmap, MT6338_AUD_TOP_SRAM_CON, 0x0);

	usleep_range(250, 270);
	/* Audio system digital clock power down release */
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON0, 0x00);
	usleep_range(250, 270);

	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON46, 0x3);

	/* Audio system digital clock power down release */
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON1, 0x0);
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON2, 0x0);
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON3, 0x0);
	/* 2ND DL sdm fifo clock*/
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON11_L, 0x6);
	/* 2ND DL scramber */
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON9_H, 0xCB);
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON9_L, 0xA1);
	/* 2ND DL sdm */
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON11_L, 0x3);
	regmap_write(priv->regmap, MT6338_AFUNC_AUD_CON11_L, 0xb);
	/* 2ND DL select 8bit 2nd SDM  */
	regmap_write(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON_H, 0x0);
	/* 2ND DL select FS = 48KHz */
	regmap_write(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0_H, 0x8f);
	/* Afe on */
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON0, 0x1);
	/* turn on DL */
	regmap_write(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0, 0x1);

	/*Reduce ESD resistance of AU_REFN */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON10, 0x8);

	/* Enable AUDGLB */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON34, 0x1);

	/* LDO dual vout selection to 1.6V */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x0);
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON36, 0x0);
	/* Select LCLDO_BUF_L /HCLDO_BUF_L/LCLDO_DACSW_L */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON39, 0x15);
	/* Enable for V32REFGEN */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON35, 0x7);
	/* Enable LCLDO_BUF_L /HCLDO_BUF_L/LCLDO_DACSW_L */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON37, 0x15);
	/* NCP */
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG1, 0xCB);
	regmap_write(priv->regmap, MT6338_AFE_NCP_CFG0, 0x1);
	usleep_range(250, 270);

	/* NVREG_DACSW vout selection, NVREG_HCBUF vout selection*/
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON33, 0x0);
	/* NVREG_LCBUF vout selection*/
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON34, 0x0);
	/* Enable for NVREG */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON41, 0x15);
	usleep_range(100, 120);

	/* Enable AUD_ZCD */
	regmap_write(priv->regmap, MT6338_ZCD_CON0, 0x1);
	/* ZCD input select HS*/
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON45, 0x2);
	regmap_write(priv->regmap, MT6338_ZCD_CON0, 0xf);
	/* Disable handset short-ckt protection*/
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON20, 0x8);
	/* Enable IBIST */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON31, 0x2);

	/*Set HS DR bias current optimization 010: 6uA */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON30, 0x22);
	/* Set HP & ZCD bias current optimization 01: ZCD: 4uA, HP/HS/LO: 5uA*/
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON32, 0x55);
	/* Set HS STB enhance circuits */
	/* MT6338_AUDDEC_PMU_CON32 */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON23, 0xf);
	/* Enable HS driver bias/core circuits*/
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON20, 0xb);
	/* Disable FBR trim */
	regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON11, 0x2);
	/* Set HS gain */
	regmap_write(priv->regmap, MT6338_ZCD_CON3, 0x9);

	/* Disable HS/LO DAC Current Trim Function */
	regmap_write(priv->regmap, MT6338_AUDDEC_2_PMU_CON9, 0x0);
	/* Enable Audio HS DAC */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON0, 0x2);
	usleep_range(100, 120);

	/* AVDD30_DAC power switch select to AVDD30_AUD */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON43, 0x1);
	/* Enable low-noise mode of DAC (Normal DAC) */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON47, 0x2);
	/* Switch HS MUX to audio DACL */
	regmap_write(priv->regmap, MT6338_AUDDEC_PMU_CON22, 0x2);

	return 0;
}
static int mt6338_mtkaif_stress_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int enable = ucontrol->value.integer.value[0];
	int value;

	dev_info(priv->dev, "%s(),  enable = %d\n",
		 __func__, enable);

	if (enable) {
		dev_info(priv->dev, "Config MT6338_AFE_MTKAIFV4_TX_CFG --> enable bypass src mode, 32K,enable loopback test 2\n");
		regmap_write(priv->regmap, MT6338_AFE_MTKAIFV4_TX_CFG, 0x34);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0, 0x3);
		regmap_write(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0, 0x3);
	} else {
		dev_info(priv->dev, "Config MT6338_AFE_MTKAIFV4_TX_CFG --> disable bypass src mode, 32K, disable loopback test 2\n");
		regmap_write(priv->regmap, MT6338_AFE_MTKAIFV4_TX_CFG, 0x14);
		regmap_write(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0, 0x0);
		regmap_write(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0, 0x0);
	}
	regmap_read(priv->regmap, MT6338_AFE_MTKAIFV4_TX_CFG, &value);
	dev_info(priv->dev, "%s(), MT6359_AFE_ADDA_MTKAIF_CFG0 = 0x%x\n",
		 __func__, value);

	dev_info(priv->dev, "%s(),	done\n", __func__);
	return 0;
}

static int mt6338_mtkaif_stress_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	int value;

	regmap_read(priv->regmap, MT6338_AFE_MTKAIFV4_TX_CFG, &value);
	dev_info(priv->dev, "%s(), MT6338_AFE_MTKAIFV4_TX_CFG = 0x%x\n",
		 __func__, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0, &value);
	dev_info(priv->dev, "%s(), MT6338_AFE_ADDA_UL_SRC_CON0_0 = 0x%x\n",
		 __func__, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0, &value);
	dev_info(priv->dev, "%(), MT6338_AFE_ADDA6_UL_SRC_CON0_0 = 0x%x\n",
		 __func__, value);

	return 0;
}

static const struct snd_kcontrol_new mt6338_snd_misc_controls[] = {
	SOC_ENUM_EXT("Headphone Plugged In", misc_control_enum[0],
		hp_plugged_in_get, hp_plugged_in_set),
	SOC_SINGLE_EXT("Audio HP ImpeDance Setting",
		SND_SOC_NOPM, 0, 0x10000, 0,
		hp_impedance_get, NULL),
	SOC_ENUM_EXT("PMIC_REG_CLEAR", misc_control_enum[0],
		NULL, mt6338_rcv_dcc_set),
	SOC_ENUM_EXT("DMic Used", misc_control_enum[0], dmic_used_get, NULL),
	SOC_ENUM_EXT("HP Hifi mode", misc_control_enum[1],
		hp_hifi_mode_get, hp_hifi_mode_set),
	SOC_ENUM_EXT("MIC Hifi mode", misc_control_enum[0],
		mic_hifi_mode_get, mic_hifi_mode_set),
	SOC_ENUM_EXT("MIC ULCF EN", misc_control_enum[0],
		mic_ulcf_en_get, mic_ulcf_en_set),
	SOC_ENUM_EXT("Pmic_Mtkaif_Stress_Switch", misc_control_enum[0],
		mt6338_mtkaif_stress_get, mt6338_mtkaif_stress_set),
};

static void keylock_set(struct mt6338_priv *priv)
{
/*
	regmap_write(priv->regmap, MT6338_TOP_DIG_WPK, 0x0);
	regmap_write(priv->regmap, MT6338_TOP_DIG_WPK_H, 0x0);
	regmap_write(priv->regmap, MT6338_TOP_TMA_KEY_H, 0x0);
	regmap_write(priv->regmap, MT6338_TOP_TMA_KEY_H, 0x0);
	regmap_write(priv->regmap, MT6338_PSC_WPK_L, 0x0);
	regmap_write(priv->regmap, MT6338_PSC_WPK_H, 0x0);
	regmap_write(priv->regmap, MT6338_HK_TOP_WKEY_L, 0x0);
	regmap_write(priv->regmap, MT6338_HK_TOP_WKEY_H, 0x0);
*/
}

static void keylock_reset(struct mt6338_priv *priv)
{
	/* key unlock */
	regmap_write(priv->regmap, MT6338_TOP_DIG_WPK, 0x38);
	regmap_write(priv->regmap, MT6338_TOP_DIG_WPK_H, 0x63);
	regmap_write(priv->regmap, MT6338_TOP_TMA_KEY, 0xc7);
	regmap_write(priv->regmap, MT6338_TOP_TMA_KEY_H, 0x9c);
	regmap_write(priv->regmap, MT6338_PSC_WPK_L, 0x29);
	regmap_write(priv->regmap, MT6338_PSC_WPK_H, 0x47);
	regmap_write(priv->regmap, MT6338_HK_TOP_WKEY_L, 0x38);
	regmap_write(priv->regmap, MT6338_HK_TOP_WKEY_H, 0x63);
}

static void codec_gpio_init(struct mt6338_priv *priv)
{
#if !defined(MTKAIFV4_SUPPORT)
	regmap_update_bits(priv->regmap, MT6338_GPIO_MODE4, 0x3 << 4, 0x1 << 4);
	// GPIO_MODE4[6:4] : 0x1, choose TDMIN_BCK
	regmap_update_bits(priv->regmap, MT6338_GPIO_MODE4, 0x3 << 4, 0x1 << 4);
	// GPIO_MODE5[2:0] : 0x1, choose TDMIN_LRCK
	regmap_update_bits(priv->regmap, MT6338_GPIO_MODE5, 0x3, 0x1);
	// GPIO_MODE2[2:0] : 0x2, choose TDMIN_DATA0
	regmap_update_bits(priv->regmap, MT6338_GPIO_MODE2, 0x3, 0x2);
	// GPIO_MODE2[6:4] : 0x2, choose TDMIN_DATA1
	regmap_update_bits(priv->regmap, MT6338_GPIO_MODE2, 0x3 << 4, 0x2 << 4);
	// GPIO_MODE3[6:4] : 0x2, choose TDMOUT_BCK
	regmap_update_bits(priv->regmap, MT6338_GPIO_MODE3, 0x3 << 4, 0x2 << 4);
	// GPIO_MODE4[2:0] : 0x1, choose TDMOUT_LRCK
	regmap_update_bits(priv->regmap, MT6338_GPIO_MODE4, 0x3, 0x1);
	// GPIO_MODE3[2:0] : 0x2, choose TDMOUT_DATA0
	regmap_update_bits(priv->regmap, MT6338_GPIO_MODE3, 0x3, 0x2);
#endif
}

static int mt6338_codec_init_reg(struct mt6338_priv *priv)
{
#if !defined(MTKAIFV4_SUPPORT)
	unsigned int sample_rate = MT6338_AFE_ETDM_48000HZ;
#endif
	unsigned int value;

	dev_info(priv->dev, "%s()", __func__);

	codec_gpio_init(priv);
	//turn on CLKSQ_PMU_CON0
	regmap_write(priv->regmap, MT6338_CLKSQ_PMU_CON0, 0xe);

#if !defined(MTKAIFV4_SUPPORT)
	regmap_update_bits(priv->regmap, MT6338_AFE_TOP_CON0, 0x1, 0x1);
	regmap_update_bits(priv->regmap, MT6338_AUDIO_TOP_CON0,
		0x1 << 0x7, 0x0 << 0x7);

	 /* reg_lrck_reset */
	 // disable ETDM OUT0
	regmap_update_bits(priv->regmap, MT6338_ETDM_OUT0_CON1_1, 0xff, 0x95);
	 // disable ETDM IN0
	regmap_update_bits(priv->regmap, MT6338_ETDM_IN0_CON1_1, 0xff, 0x95);
	/* Disable ETDM (PMIC part) first */
	// disable ETDM OUT0
	regmap_update_bits(priv->regmap, MT6338_ETDM_OUT0_CON0_0, 0x1, 0x0);
	 // disable ETDM IN0
	regmap_update_bits(priv->regmap, MT6338_ETDM_IN0_CON0_0, 0x1, 0x0);

	// ETDM_IN0_CON2
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON2_0, 0x42);
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON2_1, 0x80);
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON2_2, 0x10);
	// [31] reg_multi_ip_mode : 0: one pin multi ch, 1: multi input 2ch
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON2_3, 0x84);

	// reg_lelatch_1x_en_sel : 48k
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON4_0, 0x00);
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON4_1, 0x01);
	// bit[0]: reg_slave_bck_inv[0]
	//mt6338_i2c_write_word(priv->i2c, MT6338_ETDM_IN0_CON4_2, 0xA1);
	if (priv->dl_rate[0] != 0)
		sample_rate = mt6338_etdm_rate_transform(priv->dl_rate[0]);
	dev_info(priv->dev, "%s() dl sample_rate = %d", __func__, sample_rate);
	// bit[7:4]: reg_relatch_1x_en_sel[3:0]
	regmap_update_bits(priv->regmap, MT6338_ETDM_IN0_CON4_2,
		0xF << 0x4, (sample_rate%16) << 0x4);
	regmap_update_bits(priv->regmap, MT6338_ETDM_IN0_CON4_3,
		0xF, sample_rate/16);


	// [8] use aifo enable: 0 disable, 1 enable
	// [7:0] afifo mode: 48k
	// bit[4:0]: reg_afifo_mode[4:0]
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON8_0, 0x0A);
	// bit[4:0]: reg_afifo_mode[4:0]
	regmap_update_bits(priv->regmap, MT6338_ETDM_IN0_CON8_0,
		0x1F, sample_rate);
	// bit[0]: 1: use aifo enable, 0: disable
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON8_1, 0x41);
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON8_2, 0x00);
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON8_3, 0x40);

	// config ETDM IN0
	//[27:23] : channel number 1:2CH 3:4CH
	/* [30:28] : relatch_clock_source_sel 0/3: etdm_in0 relatch 1x_en 26m
	 * 1: IN0 slave lrck sync 26m 1x en
	 * 2: OUT0 slave lrck sync 26m 1x_en
	 */
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON0_1, 0xF9);
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON0_2, 0x9F);
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON0_3, 0x10);
	regmap_write(priv->regmap, MT6338_ETDM_IN0_CON0_0, 0x60);

	// reg_lelatch_1x_en_sel : 48k
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON4_0, 0x00);
	// bit[3] : reg_async_reset, "0: Normal work, 1: Asynchronous reset valid"
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON4_1, 0x04);
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON4_2, 0x00);
	// bit[5:1]: reg_relatch_en_sel
	//mt6338_i2c_write_word(priv->i2c, MT6338_ETDM_OUT0_CON4_3, 0x0A);
	// bit[5:1]: reg_relatch_en_sel
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON4_3, 0x00);

	if (priv->ul_rate[0] != 0)
		sample_rate = mt6338_etdm_rate_transform(priv->ul_rate[0]);

	dev_info(priv->dev, "%s() ul sample_rate = %d", __func__, sample_rate);
	regmap_update_bits(priv->regmap, MT6338_ETDM_OUT0_CON4_3,
		0x1F, sample_rate); //bit[5:1]: reg_relatch_en_sel

	// ETDM slave BCK inverse or not.
	// [7]reg_slave_bck_inv: 0 not inv, 1 inverse
	//mt6338_i2c_write_word(priv->i2c, MT6338_ETDM_OUT0_CON5_0, 0x80);

	// [8] use aifo enable: 0 disable, 1 enable
	// [7:0] afifo mode: 48k
	// bit[4:0]: reg_afifo_mode[4:0]
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON9_0, 0x0A);
	// bit[4:0]: reg_afifo_mode[4:0]
	regmap_update_bits(priv->regmap, MT6338_ETDM_OUT0_CON9_0,
		0x1F, sample_rate);
	// bit[0]: 1: use aifo enable, 0: disable
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON9_1, 0x41);
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON9_2, 0x00);
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON9_3, 0x40);

	// enable ETDM OUT0
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON0_1, 0xF9);
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON0_2, 0x9F);
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON0_3, 0x00);
	regmap_write(priv->regmap, MT6338_ETDM_OUT0_CON0_0, 0x60);

	// config loopback :
	// ETDM_0_3_COWORK_CON0_0 [3:0] eTDM OUT0 data sel
	// 0: eTDM in0
	regmap_update_bits(priv->regmap, MT6338_ETDM_0_3_COWORK_CON0_0,
		0xF, 0x8);
	//vETDM_InOutLoopback(is_lpbk);
	regmap_update_bits(priv->regmap, MT6338_ETDM_0_3_COWORK_CON0_0,
		0xF, 0x0);

	regmap_write(priv->regmap, MT6338_ETDM_0_3_COWORK_CON0_1, 0x59);
	regmap_write(priv->regmap, MT6338_ETDM_0_3_COWORK_CON0_2, 0x01);
	regmap_write(priv->regmap, MT6338_ETDM_0_3_COWORK_CON0_3, 0x11);

	regmap_write(priv->regmap, MT6338_ETDM_0_3_COWORK_CON1_0, 0x88);
	regmap_write(priv->regmap, MT6338_ETDM_0_3_COWORK_CON1_1, 0x00);
	regmap_write(priv->regmap, MT6338_ETDM_0_3_COWORK_CON1_2, 0x00);
	regmap_write(priv->regmap, MT6338_ETDM_0_3_COWORK_CON1_3, 0x00);

	// Enable ETDM (PMIC part)
	// enable ETDM IN0
	regmap_update_bits(priv->regmap, MT6338_ETDM_IN0_CON0_0, 0x1, 0x1);
	// enable ETDM OUT0
	regmap_update_bits(priv->regmap, MT6338_ETDM_OUT0_CON0_0, 0x1, 0x1);
#else
	regmap_update_bits(priv->regmap, MT6338_AUD_TOP_CKSEL_CON0,
		0x3 << 0x2, 0x0 << 0x2);
#ifdef MTKAIF_DEBUG
	/* afe on */
	regmap_update_bits(priv->regmap, MT6338_AFE_TOP_CON0, 0x1, 0x1);
	/* power on afe ctl*/
	regmap_update_bits(priv->regmap, MT6338_AUDIO_TOP_CON0, 0x1 << 0x7, 0x0 << 0x7);

	regmap_write(priv->regmap, MT6338_AFE_AUD_PAD_TOP, 0x39);
	/* txif mtkaifv4_txif_protocol3 */
	regmap_update_bits(priv->regmap, MT6338_AFE_MTKAIFV4_TX_CFG,
		0x1 << 0x2, 0x1 << 0x2);

	/* config mtkaif_rx1 (pmic), 2ch */
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0,
		0x1, 0x1);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0,
		0x1 << 0x1, 0x0 << 0x1);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0,
		0x1 << 0x2, 0x0 << 0x2);
	if (priv->dl_rate[0] != 0)
		sample_rate = mt6338_etdm_rate_transform(priv->dl_rate[0]);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0,
		0x1F << 0x3, sample_rate << 0x3);

	/* config mtkaif_rx2 (pmic), 2ch */
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0,
		0x1, 0x1);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0,
		0x1 << 0x1, 0x0 << 0x1);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0,
		0x1 << 0x2, 0x0 << 0x2);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0,
		0x1F << 0x3, sample_rate << 0x3);

	/* config mtkaif_tx1 (pmic), 4ch */
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_TX_CFG0,
		0x1, 0x1);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_TX_CFG0,
		0x1 << 0x1, 0x1 << 0x1);
	if (priv->ul_rate[0] != 0)
		sample_rate = mt6338_etdm_rate_transform(priv->ul_rate[0]);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_TX_CFG0,
		0x1F << 0x3, sample_rate << 0x3);

	/* config mtkaif_tx2 (pmic), 4ch */
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_TX_CFG0,
		0x1, 0x1);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_TX_CFG0,
		0x1 << 0x1, 0x1 << 0x1);
	regmap_update_bits(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_TX_CFG0,
		0x1F << 0x3, sample_rate << 0x3);
#endif
#endif
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON0, 0x11);
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON1, 0xff);
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON2, 0xd7);
	regmap_write(priv->regmap, MT6338_AUDIO_TOP_CON3, 0x02);

	/* set gpio */
	mt6338_set_gpio_smt(priv);
	mt6338_set_gpio_driving(priv);

	/* hp gain ctl default choose ZCD */
	priv->hp_gain_ctl = HP_GAIN_CTL_ZCD;
#ifdef NLE_IMP
	hp_gain_ctl_select(priv, priv->hp_gain_ctl);
#endif
	/* hp hifi mode, default normal mode */
	priv->hp_hifi_mode = 0;
	/* mic hifi mode, default hifi mode */
	priv->mic_hifi_mode = 0;


	regmap_read(priv->regmap, MT6338_STRUP_ELR_1, &value);

	priv->vd105 = ((value >> 0x3) & 0x1f) + 0xc;

	/* this will trigger widget "DC trim" power down event */
	enable_trim_buf(priv, true);

	regmap_write(priv->regmap, MT6338_AUDENC_PMU_CON71, 0x0);

	return 0;
}

static int mt6338_codec_probe(struct snd_soc_component *cmpnt)
{
	struct mt6338_priv *priv = snd_soc_component_get_drvdata(cmpnt);
	struct snd_soc_card *sndcard = cmpnt->card;
	struct snd_card *card = sndcard->snd_card;
	int ret = 0;

	dev_info(priv->dev, "%s(), priv->dev name %s\n",
		 __func__, dev_name(priv->dev));

#if IS_ENABLED(CONFIG_MT6338_EFUSE)
	mt6338_get_hw_ver(priv);
	set_idac_trim_val(priv);
#ifdef NLE_IMP
	mt6338_get_ln_gain(priv);
#endif
#endif
	codec_dev_attr_reg.private = priv;
	ret = snd_card_add_dev_attr(card, &codec_bin_attr_group);
	if (ret)
		pr_info("%s snd_card_add_dev_attr fail\n", __func__);

	snd_soc_component_init_regmap(cmpnt, priv->regmap);

	/* add codec misc controls */
	snd_soc_add_component_controls(cmpnt,
				       mt6338_snd_misc_controls,
				       ARRAY_SIZE(mt6338_snd_misc_controls));
	/* add vow controls */
	snd_soc_add_component_controls(cmpnt,
				       mt6338_snd_vow_controls,
				       ARRAY_SIZE(mt6338_snd_vow_controls));

	priv->hp_current_calibrate_val = get_hp_current_calibrate_val(priv);

	return mt6338_codec_init_reg(priv);
}

static void mt6338_codec_remove(struct snd_soc_component *cmpnt)
{
	snd_soc_component_exit_regmap(cmpnt);
}

static const struct snd_soc_component_driver mt6338_soc_component_driver = {
	.name = CODEC_MT6338_NAME,
	.probe = mt6338_codec_probe,
	.remove = mt6338_codec_remove,
	.controls = mt6338_snd_controls,
	.num_controls = ARRAY_SIZE(mt6338_snd_controls),
	.dapm_widgets = mt6338_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6338_dapm_widgets),
	.dapm_routes = mt6338_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt6338_dapm_routes),
};

/* debugfs */
static void codec_write_reg(struct mt6338_priv *priv, void *arg)
{
	char *token1 = NULL, *token2 = NULL;
	char *temp = arg;
	char delim[] = " ,";
	unsigned int reg_addr = 0;
	unsigned int reg_value = 0;
	int ret = 0;

	token1 = strsep(&temp, delim);
	token2 = strsep(&temp, delim);
	dev_info(priv->dev, "%s(), token1 = %s, token2 = %s, temp = %s\n",
		 __func__, token1, token2, temp);

	if ((token1 != NULL) && (token2 != NULL)) {
		ret = kstrtouint(token1, 16, &reg_addr);
		ret = kstrtouint(token2, 16, &reg_value);
		dev_info(priv->dev, "%s(), reg_addr = 0x%x, reg_value = 0x%x\n",
			 __func__,
			 reg_addr, reg_value);
		regmap_write(priv->regmap, reg_addr, reg_value);
		regmap_read(priv->regmap, reg_addr, &reg_value);
		dev_info(priv->dev, "%s(), reg_addr = 0x%x, reg_value = 0x%x\n",
			 __func__,
			 reg_addr, reg_value);
	} else {
		dev_err(priv->dev, "token1 or token2 is NULL!\n");
	}
}

static void debug_write_reg(struct file *file, void *arg)
{
	struct mt6338_priv *priv = file->private_data;

	return codec_write_reg(priv, arg);
}

struct command_function {
	const char *cmd;
	void (*fn)(struct file *file, void *arg);
};

#define CMD_FN(_cmd, _fn) {	\
	.cmd = _cmd,		\
	.fn = _fn,		\
}

static const struct command_function debug_cmds[] = {
	CMD_FN("write_reg", debug_write_reg),
	{}
};

static int mt6338_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mt6338_codec_read(struct mt6338_priv *priv, char *buffer, size_t size)
{
	int n = 0;
	unsigned int value = 0;

	if (!buffer)
		return -ENOMEM;

	n += scnprintf(buffer + n, size - n, "mtkaif_protocol = %d\n",
		       priv->mtkaif_protocol);
	n += scnprintf(buffer + n, size - n, "dc_trim_data:\n");
	n += scnprintf(buffer + n, size - n, "\tcalibrated = %d\n",
		       priv->dc_trim.calibrated);
	n += scnprintf(buffer + n, size - n, "\tmic_vinp_mv = %d\n",
		       priv->dc_trim.mic_vinp_mv);
	n += scnprintf(buffer + n, size - n, "\ttrim_code L = 0x%x|0x%x",
		       priv->hp_trim_3_pole.hp_fine_trim_l,
		       priv->hp_trim_3_pole.hp_trim_l);
	n += scnprintf(buffer + n, size - n, "\ttrim_code R = 0x%x|0x%x\n",
		       priv->hp_trim_3_pole.hp_fine_trim_r,
		       priv->hp_trim_3_pole.hp_trim_r);

	n += scnprintf(buffer + n, size - n, "codec_ops:\n");
	n += scnprintf(buffer + n, size - n, "\tenable_dc_compensation = %p\n",
		       priv->ops.enable_dc_compensation);
	n += scnprintf(buffer + n, size - n, "\tset_lch_dc_compensation = %p\n",
		       priv->ops.set_lch_dc_compensation);
	n += scnprintf(buffer + n, size - n, "\tset_rch_dc_compensation = %p\n",
		       priv->ops.set_rch_dc_compensation);

	n += scnprintf(buffer + n, size - n, "hp_impedance = %d\n",
		       priv->hp_impedance);
	n += scnprintf(buffer + n, size - n, "hp_current_calibrate_val = %d\n",
		       priv->hp_current_calibrate_val);

	/* Replace :regmap_read(priv->regmap  to   value = mt6338_i2c_read_byte(priv->i2c, */
	regmap_read(priv->regmap, MT6338_STRUP_ELR_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_STRUP_ELR_1 0x%x = 0x%x\n",
			   MT6338_STRUP_ELR_1, value);
	regmap_read(priv->regmap, MT6338_TOP_DIG_WPK, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_TOP_DIG_WPK 0x%x = 0x%x\n",
			   MT6338_TOP_DIG_WPK, value);
	regmap_read(priv->regmap, MT6338_TOP_DIG_WPK_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_TOP_DIG_WPK_H 0x%x = 0x%x\n",
			   MT6338_TOP_DIG_WPK_H, value);
	regmap_read(priv->regmap, MT6338_TOP_TMA_KEY_H, &value);
		n += scnprintf(buffer + n, size - n,
			   "MT6338_TOP_TMA_KEY_H 0x%x = 0x%x\n",
			   MT6338_TOP_TMA_KEY_H, value);
	regmap_read(priv->regmap, MT6338_TOP_TMA_KEY, &value);
		n += scnprintf(buffer + n, size - n,
			   "MT6338_TOP_TMA_KEY 0x%x = 0x%x\n",
			   MT6338_TOP_TMA_KEY, value);
	regmap_read(priv->regmap, MT6338_LDO_VAUD18_CON0, &value);
		n += scnprintf(buffer + n, size - n,
			   "MT6338_LDO_VAUD18_CON0 0x%x = 0x%x\n",
			   MT6338_LDO_VAUD18_CON0, value);
	regmap_read(priv->regmap, MT6338_VPLL18_PMU_CON0, &value);
		n += scnprintf(buffer + n, size - n,
			   "MT6338_VPLL18_PMU_CON0 0x%x = 0x%x\n",
			   MT6338_VPLL18_PMU_CON0, value);
	regmap_read(priv->regmap, MT6338_CLKSQ_PMU_CON0, &value);
		n += scnprintf(buffer + n, size - n,
			   "MT6338_CLKSQ_PMU_CON0 0x%x = 0x%x\n",
			   MT6338_CLKSQ_PMU_CON0, value);
	regmap_read(priv->regmap, MT6338_PLL208M_PMU_CON5, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_PLL208M_PMU_CON5 0x%x = 0x%x\n",
		       MT6338_PLL208M_PMU_CON5, value);
	regmap_read(priv->regmap, MT6338_PLL208M_PMU_CON4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_PLL208M_PMU_CON4 0x%x = 0x%x\n",
		       MT6338_PLL208M_PMU_CON4, value);
	regmap_read(priv->regmap, MT6338_TOP_CON, &value);
		n += scnprintf(buffer + n, size - n,
			   "MT6338_TOP_CON 0x%x = 0x%x\n",
			   MT6338_TOP_CON, value);
	regmap_read(priv->regmap, MT6338_DA_INTF_STTING1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_DA_INTF_STTING1 0x%x = 0x%x\n",
			   MT6338_DA_INTF_STTING1, value);
#ifdef MT6338_TOP_DEBUG
	regmap_read(priv->regmap, MT6338_SMT_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_SMT_CON1 0x%x = 0x%x\n",
		       MT6338_SMT_CON1, value);
	regmap_read(priv->regmap, MT6338_TOP_DIG_WPK, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_TOP_DIG_WPK 0x%x = 0x%x\n",
		       MT6338_TOP_DIG_WPK, value);
	regmap_read(priv->regmap, MT6338_TOP_DIG_WPK_H, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_TOP_DIG_WPK_H 0x%x = 0x%x\n",
		       MT6338_TOP_DIG_WPK_H, value);
	regmap_read(priv->regmap, MT6338_TOP_TMA_KEY_H, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_TOP_TMA_KEY_H 0x%x = 0x%x\n",
		       MT6338_TOP_TMA_KEY_H, value);
	regmap_read(priv->regmap, MT6338_PSC_WPK_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_PSC_WPK_L 0x%x = 0x%x\n",
		       MT6338_PSC_WPK_L, value);
	regmap_read(priv->regmap, MT6338_PSC_WPK_H, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_PSC_WPK_H 0x%x = 0x%x\n",
		       MT6338_PSC_WPK_H, value);
	regmap_read(priv->regmap, MT6338_HK_TOP_WKEY_L, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_HK_TOP_WKEY_L 0x%x = 0x%x\n",
		       MT6338_HK_TOP_WKEY_L, value);
	regmap_read(priv->regmap, MT6338_HK_TOP_WKEY_H, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_HK_TOP_WKEY_H 0x%x = 0x%x\n",
		       MT6338_HK_TOP_WKEY_H, value);

	regmap_read(priv->regmap, MT6338_GPIO_MODE2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_GPIO_MODE2 0x%x = 0x%x\n",
		       MT6338_GPIO_MODE2, value);
	regmap_read(priv->regmap, MT6338_GPIO_MODE3, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_GPIO_MODE3 0x%x = 0x%x\n",
		       MT6338_GPIO_MODE3, value);
	regmap_read(priv->regmap, MT6338_GPIO_MODE4, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_GPIO_MODE4 0x%x = 0x%x\n",
		       MT6338_GPIO_MODE4, value);
	regmap_read(priv->regmap, MT6338_GPIO_MODE5, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_GPIO_MODE5 0x%x = 0x%x\n",
		       MT6338_GPIO_MODE5, value);
	regmap_read(priv->regmap, MT6338_TOP_CON, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_TOP_CON 0x%x = 0x%x\n",
		       MT6338_TOP_CON, value);
	regmap_read(priv->regmap, MT6338_TEST_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_TEST_CON0 0x%x = 0x%x\n",
		       MT6338_TEST_CON0, value);
	regmap_read(priv->regmap, MT6338_SMT_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_SMT_CON0 0x%x = 0x%x\n",
		       MT6338_SMT_CON0, value);
	regmap_read(priv->regmap, MT6338_GPIO_PULLEN0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_GPIO_PULLEN0 0x%x = 0x%x\n",
		       MT6338_GPIO_PULLEN0, value);
	regmap_read(priv->regmap, MT6338_GPIO_PULLEN1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_GPIO_PULLEN1 0x%x = 0x%x\n",
		       MT6338_GPIO_PULLEN1, value);
	regmap_read(priv->regmap, MT6338_TOP_CKPDN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_TOP_CKPDN_CON0 0x%x = 0x%x\n",
		       MT6338_TOP_CKPDN_CON0, value);
	regmap_read(priv->regmap, MT6338_PLT_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_PLT_CON0 0x%x = 0x%x\n",
		       MT6338_PLT_CON0, value);
	regmap_read(priv->regmap, MT6338_PLT_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_PLT_CON1 0x%x = 0x%x\n",
		       MT6338_PLT_CON1, value);
	regmap_read(priv->regmap, MT6338_HK_TOP_CLK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_HK_TOP_CLK_CON0 0x%x = 0x%x\n",
		       MT6338_HK_TOP_CLK_CON0, value);
	regmap_read(priv->regmap, MT6338_AUXADC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_AUXADC_CON0 0x%x = 0x%x\n",
		       MT6338_AUXADC_CON0, value);
	regmap_read(priv->regmap, MT6338_AUXADC_TRIM_SEL2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_AUXADC_TRIM_SEL2 0x%x = 0x%x\n",
		       MT6338_AUXADC_TRIM_SEL2, value);
	regmap_read(priv->regmap, MT6338_TOP_TOP_CKHWEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_TOP_TOP_CKHWEN_CON0 0x%x = 0x%x\n",
		       MT6338_TOP_TOP_CKHWEN_CON0, value);
	regmap_read(priv->regmap, MT6338_LDO_TOP_CLK_DCM_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_LDO_TOP_CLK_DCM_CON0 0x%x = 0x%x\n",
		       MT6338_LDO_TOP_CLK_DCM_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_AUD_TOP_CKPDN_CON0_CLR 0x%x = 0x%x\n",
		       MT6338_AUD_TOP_CKPDN_CON0_CLR, value);
	regmap_read(priv->regmap, MT6338_LDO_TOP_VR_CLK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_LDO_TOP_VR_CLK_CON0 0x%x = 0x%x\n",
		       MT6338_LDO_TOP_VR_CLK_CON0, value);
	regmap_read(priv->regmap, MT6338_LDO_VAUD18_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_LDO_VAUD18_CON2 0x%x = 0x%x\n",
		       MT6338_LDO_VAUD18_CON2, value);
	regmap_read(priv->regmap, MT6338_TSBG_PMU_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_TSBG_PMU_CON0 0x%x = 0x%x\n",
		       MT6338_TSBG_PMU_CON0, value);
	regmap_read(priv->regmap, MT6338_STRUP_ELR_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_STRUP_ELR_0 0x%x = 0x%x\n",
		       MT6338_STRUP_ELR_0, value);
	regmap_read(priv->regmap, MT6338_CLKSQ_PMU_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "MT6338_CLKSQ_PMU_CON0 0x%x = 0x%x\n",
		       MT6338_CLKSQ_PMU_CON0, value);
#endif
	regmap_read(priv->regmap, MT6338_AUD_TOP_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_ID 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_ID, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_ID_H 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_REV0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_REV0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_DBI 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_DBI, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_DXI 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_DXI, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_TPM0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_TPM0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_TPM0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_TPM0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_TPM0_H 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_TPM0_H, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_TPM1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_TPM1 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_TPM1, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_TPM1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_TPM1_H 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_TPM1_H, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_CON0_SET 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_CON0_SET, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_CON0_CLR 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_CON0_CLR, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_CON0_H 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H_SET, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_CON0_H_SET 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_CON0_H_SET, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKPDN_CON0_H_CLR, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKPDN_CON0_H_CLR 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKPDN_CON0_H_CLR, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKSEL_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKSEL_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKSEL_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKSEL_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKSEL_CON0_SET 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKSEL_CON0_SET, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKSEL_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKSEL_CON0_CLR 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKSEL_CON0_CLR, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKTST_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKTST_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKTST_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CKTST_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CKTST_CON0_H 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CKTST_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CLK_HWEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CLK_HWEN_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CLK_HWEN_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CLK_HWEN_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CLK_HWEN_CON0_SET 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CLK_HWEN_CON0_SET, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_CLK_HWEN_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_CLK_HWEN_CON0_CLR 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_CLK_HWEN_CON0_CLR, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_RST_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_RST_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_RST_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_RST_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_RST_CON0_SET 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_RST_CON0_SET, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_RST_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_RST_CON0_CLR 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_RST_CON0_CLR, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_RST_BANK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_RST_BANK_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_RST_BANK_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_INT_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_INT_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_INT_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_INT_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_INT_CON0_SET 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_INT_CON0_SET, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_INT_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_INT_CON0_CLR 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_INT_CON0_CLR, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_INT_MASK_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_INT_MASK_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_INT_MASK_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_INT_MASK_CON0_SET, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_INT_MASK_CON0_SET 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_INT_MASK_CON0_SET, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_INT_MASK_CON0_CLR, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_INT_MASK_CON0_CLR 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_INT_MASK_CON0_CLR, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_INT_STATUS0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_INT_STATUS0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_INT_STATUS0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_INT_RAW_STATUS0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_INT_RAW_STATUS0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_INT_RAW_STATUS0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_INT_MISC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_INT_MISC_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_INT_MISC_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_MON_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_MON_CON0 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_MON_CON0, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_MON_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_MON_CON0_H 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_MON_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_CFG 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_CFG, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_CFG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_CFG_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_CFG_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_CFG1 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_AUD_PAD_TOP, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_AUD_PAD_TOP 0x%x = 0x%x\n",
			   MT6338_AFE_AUD_PAD_TOP, value);
	regmap_read(priv->regmap, MT6338_AFE_AUD_PAD_TOP_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_AUD_PAD_TOP_MON 0x%x = 0x%x\n",
			   MT6338_AFE_AUD_PAD_TOP_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_AUD_PAD_TOP_MON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_AUD_PAD_TOP_MON_H 0x%x = 0x%x\n",
			   MT6338_AFE_AUD_PAD_TOP_MON_H, value);
	regmap_read(priv->regmap, MT6338_AFE_AUD_PAD_TOP_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_AUD_PAD_TOP_MON1 0x%x = 0x%x\n",
			   MT6338_AFE_AUD_PAD_TOP_MON1, value);
	regmap_read(priv->regmap, MT6338_AFE_AUD_PAD_TOP_MON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_AUD_PAD_TOP_MON1_H 0x%x = 0x%x\n",
			   MT6338_AFE_AUD_PAD_TOP_MON1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_AUD_PAD_TOP_MON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_AUD_PAD_TOP_MON2 0x%x = 0x%x\n",
			   MT6338_AFE_AUD_PAD_TOP_MON2, value);
	regmap_read(priv->regmap, MT6338_AUD_TOP_SRAM_CON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUD_TOP_SRAM_CON 0x%x = 0x%x\n",
			   MT6338_AUD_TOP_SRAM_CON, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK1_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK1_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK1_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK1_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK1_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK1_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK1_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK1_CFG2 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK1_CFG2, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK2_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK2_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK2_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK2_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK2_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK2_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK2_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK2_CFG2 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK2_CFG2, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK3_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK3_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK3_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK3_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK3_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK3_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK3_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK3_CFG2 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK3_CFG2, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK4_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK4_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK4_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK4_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK4_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK4_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_DCCLK4_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DCCLK4_CFG2 0x%x = 0x%x\n",
			   MT6338_AFE_DCCLK4_CFG2, value);
	regmap_read(priv->regmap, MT6338_AO_AFUNC_AUD_CON3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AO_AFUNC_AUD_CON3_L 0x%x = 0x%x\n",
			   MT6338_AO_AFUNC_AUD_CON3_L, value);
	regmap_read(priv->regmap, MT6338_AO_AFUNC_AUD_CON4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AO_AFUNC_AUD_CON4_H 0x%x = 0x%x\n",
			   MT6338_AO_AFUNC_AUD_CON4_H, value);
	regmap_read(priv->regmap, MT6338_AO_AFUNC_AUD_CON4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AO_AFUNC_AUD_CON4_L 0x%x = 0x%x\n",
			   MT6338_AO_AFUNC_AUD_CON4_L, value);
	regmap_read(priv->regmap, MT6338_AO_AFUNC_AUD_CON7_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AO_AFUNC_AUD_CON7_H 0x%x = 0x%x\n",
			   MT6338_AO_AFUNC_AUD_CON7_H, value);
	regmap_read(priv->regmap, MT6338_AO_AFUNC_AUD_CON7_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AO_AFUNC_AUD_CON7_L 0x%x = 0x%x\n",
			   MT6338_AO_AFUNC_AUD_CON7_L, value);
	regmap_read(priv->regmap, MT6338_AO_AFE_DMIC_ARRAY_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AO_AFE_DMIC_ARRAY_CFG 0x%x = 0x%x\n",
			   MT6338_AO_AFE_DMIC_ARRAY_CFG, value);
	regmap_read(priv->regmap, MT6338_AO_AFE_ADC_ASYNC_FIFO_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AO_AFE_ADC_ASYNC_FIFO_CFG 0x%x = 0x%x\n",
			   MT6338_AO_AFE_ADC_ASYNC_FIFO_CFG, value);
	regmap_read(priv->regmap, MT6338_AO_AUDIO_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AO_AUDIO_TOP_CON0 0x%x = 0x%x\n",
			   MT6338_AO_AUDIO_TOP_CON0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_DSN_DXI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_TOP_CON0 0x%x = 0x%x\n",
			   MT6338_AUDIO_TOP_CON0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_TOP_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_TOP_CON1 0x%x = 0x%x\n",
			   MT6338_AUDIO_TOP_CON1, value);
	regmap_read(priv->regmap, MT6338_AUDIO_TOP_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_TOP_CON2 0x%x = 0x%x\n",
			   MT6338_AUDIO_TOP_CON2, value);
	regmap_read(priv->regmap, MT6338_AUDIO_TOP_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_TOP_CON3 0x%x = 0x%x\n",
			   MT6338_AUDIO_TOP_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_TOP_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_TOP_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_MON_DEBUG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_MON_DEBUG0 0x%x = 0x%x\n",
			   MT6338_AFE_MON_DEBUG0, value);
	regmap_read(priv->regmap, MT6338_AFE_MON_DEBUG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_MON_DEBUG1 0x%x = 0x%x\n",
			   MT6338_AFE_MON_DEBUG1, value);
	regmap_read(priv->regmap, MT6338_AFE_MTKAIF_MUX_CFG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_MTKAIF_MUX_CFG_H 0x%x = 0x%x\n",
			   MT6338_AFE_MTKAIF_MUX_CFG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_MTKAIF_MUX_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_MTKAIF_MUX_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_MTKAIF_MUX_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_SINEGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_SINEGEN_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_SINEGEN_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_SINEGEN_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_SINEGEN_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_SINEGEN_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_SINEGEN_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_SINEGEN_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_SINEGEN_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_SINEGEN_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_SINEGEN_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_SINEGEN_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_SINEGEN_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_SINEGEN_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_SINEGEN_CON4, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_STF_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_STF_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_STF_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_STF_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_COEFF, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_COEFF 0x%x = 0x%x\n",
			   MT6338_AFE_STF_COEFF, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_COEFF_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_COEFF_M 0x%x = 0x%x\n",
			   MT6338_AFE_STF_COEFF_M, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_COEFF_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_COEFF_H 0x%x = 0x%x\n",
			   MT6338_AFE_STF_COEFF_H, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_GAIN, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_GAIN 0x%x = 0x%x\n",
			   MT6338_AFE_STF_GAIN, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_GAIN_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_GAIN_M 0x%x = 0x%x\n",
			   MT6338_AFE_STF_GAIN_M, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_GAIN_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_GAIN_H 0x%x = 0x%x\n",
			   MT6338_AFE_STF_GAIN_H, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_COEFF_RD, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_COEFF_RD 0x%x = 0x%x\n",
			   MT6338_AFE_STF_COEFF_RD, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_MON 0x%x = 0x%x\n",
			   MT6338_AFE_STF_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_MON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_MON_M 0x%x = 0x%x\n",
			   MT6338_AFE_STF_MON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_MON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_MON_H 0x%x = 0x%x\n",
			   MT6338_AFE_STF_MON_H, value);
	regmap_read(priv->regmap, MT6338_AFE_STF_MON_H1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_STF_MON_H1 0x%x = 0x%x\n",
			   MT6338_AFE_STF_MON_H1, value);
	regmap_read(priv->regmap, MT6338_AFE_NCP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NCP_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_NCP_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_NCP_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NCP_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_NCP_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_NCP_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NCP_CFG2 0x%x = 0x%x\n",
			   MT6338_AFE_NCP_CFG2, value);
	regmap_read(priv->regmap, MT6338_AFE_NCP_CFG3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NCP_CFG3 0x%x = 0x%x\n",
			   MT6338_AFE_NCP_CFG3, value);
	regmap_read(priv->regmap, MT6338_AFE_NCP_CFG4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NCP_CFG4 0x%x = 0x%x\n",
			   MT6338_AFE_NCP_CFG4, value);
	regmap_read(priv->regmap, MT6338_AFE_TOP_DEBUG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_TOP_DEBUG0 0x%x = 0x%x\n",
			   MT6338_AFE_TOP_DEBUG0, value);
	regmap_read(priv->regmap, MT6338_AFE_MTKAIF_IN_MUX_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_MTKAIF_IN_MUX_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_MTKAIF_IN_MUX_CFG, value);
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_2ND_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_2ND_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_2ND_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_2ND_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_2ND_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_2ND_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_2ND_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_2ND_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_2ND_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_2ND_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_2ND_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_2ND_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_2ND_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_2ND_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_2ND_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_2ND_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_2ND_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_2ND_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_2ND_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_2ND_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_2ND_DSN_DXI, value);
#endif
#ifdef MT6338_GSRC_DEBUG
	regmap_read(priv->regmap, MT6338_GENERAL_ASRC_EN_ON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_GENERAL_ASRC_EN_ON 0x%x = 0x%x\n",
			   MT6338_GENERAL_ASRC_EN_ON, value);
	regmap_read(priv->regmap, MT6338_GASRC1_MODE, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_GASRC1_MODE 0x%x = 0x%x\n",
			   MT6338_GASRC1_MODE, value);
	regmap_read(priv->regmap, MT6338_GASRC2_MODE, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_GASRC2_MODE 0x%x = 0x%x\n",
			   MT6338_GASRC2_MODE, value);
	regmap_read(priv->regmap, MT6338_GASRC3_MODE, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_GASRC3_MODE 0x%x = 0x%x\n",
			   MT6338_GASRC3_MODE, value);
	regmap_read(priv->regmap, MT6338_GASRC4_MODE, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_GASRC4_MODE 0x%x = 0x%x\n",
			   MT6338_GASRC4_MODE, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON2_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON2_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON3_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON3_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON4, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON4_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON4_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON4_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON5 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON5, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON5_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON5_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON5_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON5_H0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON5_H0 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON5_H0, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON5_H1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON5_H1 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON5_H1, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON6 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON6, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON6_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON6_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON6_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON6_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON6_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON6_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON7 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON7, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON7_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON7_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON7_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON7_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON7_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON7_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON8 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON8, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON8_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON8_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON8_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON8_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON8_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON8_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON9 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON9, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON9_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON9_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON9_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON9_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON9_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON9_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON10 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON10, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON10_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON10_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON10_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON10_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON10_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON10_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON11 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON11, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON11_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON11_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON11_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON12 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON12, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON12_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON12_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON12_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON12_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON12_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON12_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON13 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON13, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON14, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON14 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON14, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON14_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON14_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON14_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON14_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON14_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON14_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC1_CON15, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC1_CON15 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC1_CON15, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON2_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON2_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON3_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON3_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON4, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON4_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON4_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON4_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON5 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON5, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON5_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON5_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON5_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON5_H0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON5_H0 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON5_H0, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON5_H1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON5_H1 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON5_H1, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON6 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON6, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON6_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON6_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON6_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON6_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON6_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON6_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON7 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON7, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON7_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON7_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON7_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON7_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON7_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON7_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON8 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON8, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON8_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON8_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON8_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON8_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON8_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON8_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON9 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON9, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON9_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON9_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON9_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON9_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON9_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON9_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON10 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON10, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON10_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON10_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON10_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON10_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON10_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON10_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON11 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON11, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON11_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON11_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON11_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON12 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON12, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON12_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON12_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON12_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON12_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON12_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON12_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON13 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON13, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON14, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON14 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON14, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON14_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON14_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON14_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON14_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON14_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON14_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC2_CON15, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC2_CON15 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC2_CON15, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC_CK_SEL, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC_CK_SEL 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC_CK_SEL, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_3RD_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_3RD_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_3RD_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_3RD_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_3RD_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_3RD_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_3RD_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_3RD_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_3RD_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_3RD_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_3RD_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_3RD_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_3RD_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_3RD_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_3RD_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_3RD_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_3RD_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_3RD_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_3RD_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_3RD_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_3RD_DSN_DXI, value);
#endif
#ifdef MT6338_GSRC_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON2_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON2_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON3_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON3_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON4, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON4_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON4_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON4_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON5 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON5, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON5_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON5_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON5_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON5_H0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON5_H0 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON5_H0, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON5_H1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON5_H1 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON5_H1, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON6 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON6, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON6_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON6_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON6_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON6_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON6_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON6_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON7 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON7, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON7_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON7_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON7_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON7_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON7_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON7_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON8 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON8, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON8_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON8_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON8_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON8_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON8_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON8_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON9 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON9, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON9_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON9_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON9_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON9_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON9_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON9_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON10 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON10, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON10_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON10_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON10_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON10_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON10_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON10_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON11 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON11, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON11_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON11_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON11_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON12 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON12, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON12_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON12_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON12_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON12_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON12_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON12_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON13 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON13, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON14, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON14 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON14, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON14_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON14_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON14_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON14_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON14_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON14_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC3_CON15, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC3_CON15 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC3_CON15, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON2_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON2_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON3_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON3_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON4, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON4_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON4_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON4_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON5 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON5, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON5_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON5_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON5_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON5_H0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON5_H0 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON5_H0, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON5_H1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON5_H1 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON5_H1, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON6 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON6, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON6_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON6_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON6_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON6_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON6_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON6_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON7 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON7, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON7_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON7_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON7_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON7_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON7_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON7_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON8 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON8, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON8_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON8_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON8_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON8_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON8_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON8_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON9 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON9, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON9_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON9_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON9_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON9_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON9_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON9_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON10 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON10, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON10_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON10_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON10_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON10_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON10_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON10_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON11 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON11, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON11_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON11_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON11_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON12 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON12, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON12_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON12_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON12_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON12_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON12_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON12_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON13 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON13, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON14, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON14 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON14, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON14_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON14_M 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON14_M, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON14_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON14_H 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON14_H, value);
	regmap_read(priv->regmap, MT6338_AFE_GASRC4_CON15, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GASRC4_CON15 0x%x = 0x%x\n",
			   MT6338_AFE_GASRC4_CON15, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_4TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_4TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_4TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_4TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_4TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_4TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_4TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_4TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_4TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_4TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_4TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_4TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_4TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_4TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_4TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_4TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_4TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_4TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_4TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_4TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_4TH_DSN_DXI, value);
#endif
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_DL_CON0_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_DL_CON0_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_DL_CON0_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_DL_CON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_DL_CON0_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_DL_CON0_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_DL_CON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_DL_CON0_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_DL_CON0_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON0_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON0_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON0_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON0_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON0_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON0_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON0_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON0_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON1_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON1_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON1_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON1_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON1_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON1_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON1_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON1_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON2_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON2_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON2_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON2_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON2_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON2_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON2_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON2_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON2_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_CON2_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_CON2_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_CON2_0, value);
#ifdef MT6338_IIR_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_02_01_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_02_01_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_02_01_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_02_01_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_02_01_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_02_01_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_02_01_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_02_01_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_02_01_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_02_01_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_02_01_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_02_01_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_04_03_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_04_03_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_04_03_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_04_03_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_04_03_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_04_03_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_04_03_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_04_03_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_04_03_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_04_03_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_04_03_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_04_03_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_06_05_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_06_05_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_06_05_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_06_05_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_06_05_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_06_05_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_06_05_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_06_05_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_06_05_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_06_05_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_06_05_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_06_05_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_08_07_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_08_07_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_08_07_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_08_07_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_08_07_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_08_07_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_08_07_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_08_07_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_08_07_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_08_07_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_08_07_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_08_07_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_10_09_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_10_09_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_10_09_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_10_09_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_10_09_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_10_09_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_10_09_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_10_09_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_10_09_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_IIR_COEF_10_09_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_IIR_COEF_10_09_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_IIR_COEF_10_09_0, value);
#endif
#ifdef MT6338_ULCF_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_02_01_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_02_01_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_02_01_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_02_01_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_02_01_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_02_01_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_02_01_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_02_01_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_02_01_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_04_03_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_04_03_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_04_03_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_04_03_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_04_03_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_04_03_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_04_03_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_04_03_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_04_03_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_06_05_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_06_05_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_06_05_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_06_05_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_06_05_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_06_05_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_06_05_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_06_05_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_06_05_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_08_07_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_08_07_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_08_07_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_08_07_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_08_07_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_08_07_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_08_07_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_08_07_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_08_07_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_10_09_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_10_09_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_10_09_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_10_09_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_10_09_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_10_09_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_10_09_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_10_09_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_10_09_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_12_11_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_12_11_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_12_11_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_12_11_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_12_11_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_12_11_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_12_11_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_12_11_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_12_11_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_14_13_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_14_13_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_14_13_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_14_13_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_14_13_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_14_13_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_14_13_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_14_13_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_14_13_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_16_15_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_16_15_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_16_15_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_16_15_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_16_15_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_16_15_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_16_15_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_16_15_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_16_15_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_18_17_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_18_17_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_18_17_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_18_17_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_18_17_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_18_17_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_18_17_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_18_17_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_18_17_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_20_19_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_20_19_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_20_19_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_20_19_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_20_19_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_20_19_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_20_19_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_20_19_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_20_19_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_22_21_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_22_21_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_22_21_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_22_21_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_22_21_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_22_21_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_22_21_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_22_21_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_22_21_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_24_23_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_24_23_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_24_23_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_24_23_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_24_23_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_24_23_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_24_23_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_24_23_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_24_23_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_26_25_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_26_25_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_26_25_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_26_25_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_26_25_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_26_25_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_26_25_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_26_25_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_26_25_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_28_27_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_28_27_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_28_27_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_28_27_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_28_27_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_28_27_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_28_27_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_28_27_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_28_27_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_30_29_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_30_29_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_30_29_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_30_29_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_30_29_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_30_29_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_30_29_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_30_29_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_30_29_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_32_31_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_32_31_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_32_31_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_32_31_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_32_31_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_32_31_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_ULCF_CFG_32_31_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_ULCF_CFG_32_31_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_ULCF_CFG_32_31_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_MON0_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_MON0_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_MON0_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_MON0_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_MON0_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_MON0_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_MON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_MON0_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_MON0_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_MON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_MON0_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_MON0_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_MON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_MON1_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_MON1_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_MON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_MON1_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_MON1_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_MON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_MON1_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_MON1_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_UL_SRC_MON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_UL_SRC_MON1_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_UL_SRC_MON1_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_SRC_DEBUG_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_SRC_DEBUG_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_SRC_DEBUG_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_SRC_DEBUG_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_SRC_DEBUG_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_SRC_DEBUG_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_SRC_DEBUG_MON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_SRC_DEBUG_MON0_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_SRC_DEBUG_MON0_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_SRC_DEBUG_MON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_SRC_DEBUG_MON0_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_SRC_DEBUG_MON0_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON0_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON0_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON0_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON0_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON0_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON0_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON0_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON0_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON1_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON1_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON1_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON1_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON1_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON1_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON1_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON1_0, value);
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_5TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_5TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_5TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_5TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_5TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_5TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_5TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_5TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_5TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_5TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_5TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_5TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_5TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_5TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_5TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_5TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_5TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_5TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_5TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_5TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_5TH_DSN_DXI, value);
#endif
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON2_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON2_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON2_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON2_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON2_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON2_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON2_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON2_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON2_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_CON2_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_CON2_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_CON2_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_02_01_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_02_01_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_02_01_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_02_01_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_02_01_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_02_01_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_02_01_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_02_01_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_02_01_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_02_01_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_02_01_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_02_01_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_04_03_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_04_03_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_04_03_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_04_03_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_04_03_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_04_03_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_04_03_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_04_03_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_04_03_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_04_03_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_04_03_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_04_03_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_06_05_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_06_05_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_06_05_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_06_05_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_06_05_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_06_05_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_06_05_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_06_05_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_06_05_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_06_05_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_06_05_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_06_05_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_08_07_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_08_07_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_08_07_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_08_07_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_08_07_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_08_07_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_08_07_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_08_07_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_08_07_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_08_07_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_08_07_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_08_07_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_10_09_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_10_09_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_10_09_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_10_09_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_10_09_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_10_09_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_10_09_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_10_09_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_10_09_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_IIR_COEF_10_09_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_IIR_COEF_10_09_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_IIR_COEF_10_09_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_02_01_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_02_01_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_02_01_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_02_01_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_02_01_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_02_01_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_02_01_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_02_01_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_02_01_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_02_01_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_02_01_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_02_01_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_04_03_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_04_03_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_04_03_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_04_03_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_04_03_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_04_03_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_04_03_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_04_03_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_04_03_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_04_03_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_04_03_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_04_03_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_06_05_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_06_05_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_06_05_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_06_05_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_06_05_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_06_05_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_06_05_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_06_05_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_06_05_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_06_05_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_06_05_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_06_05_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_08_07_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_08_07_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_08_07_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_08_07_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_08_07_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_08_07_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_08_07_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_08_07_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_08_07_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_08_07_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_08_07_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_08_07_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_10_09_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_10_09_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_10_09_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_10_09_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_10_09_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_10_09_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_10_09_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_10_09_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_10_09_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_10_09_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_10_09_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_10_09_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_12_11_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_12_11_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_12_11_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_12_11_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_12_11_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_12_11_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_12_11_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_12_11_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_12_11_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_12_11_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_12_11_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_12_11_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_14_13_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_14_13_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_14_13_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_14_13_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_14_13_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_14_13_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_14_13_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_14_13_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_14_13_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_14_13_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_14_13_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_14_13_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_16_15_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_16_15_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_16_15_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_16_15_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_16_15_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_16_15_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_16_15_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_16_15_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_16_15_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_16_15_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_16_15_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_16_15_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_18_17_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_18_17_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_18_17_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_18_17_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_18_17_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_18_17_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_18_17_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_18_17_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_18_17_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_18_17_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_18_17_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_18_17_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_20_19_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_20_19_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_20_19_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_20_19_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_20_19_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_20_19_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_20_19_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_20_19_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_20_19_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_20_19_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_20_19_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_20_19_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_22_21_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_22_21_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_22_21_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_22_21_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_22_21_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_22_21_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_22_21_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_22_21_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_22_21_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_22_21_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_22_21_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_22_21_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_24_23_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_24_23_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_24_23_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_24_23_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_24_23_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_24_23_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_24_23_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_24_23_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_24_23_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_24_23_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_24_23_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_24_23_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_26_25_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_26_25_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_26_25_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_26_25_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_26_25_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_26_25_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_26_25_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_26_25_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_26_25_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_26_25_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_26_25_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_26_25_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_28_27_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_28_27_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_28_27_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_28_27_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_28_27_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_28_27_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_28_27_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_28_27_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_28_27_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_28_27_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_28_27_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_28_27_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_30_29_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_30_29_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_30_29_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_30_29_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_30_29_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_30_29_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_30_29_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_30_29_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_30_29_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_30_29_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_30_29_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_30_29_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_32_31_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_32_31_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_32_31_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_32_31_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_32_31_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_32_31_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_32_31_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_32_31_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_32_31_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_ULCF_CFG_32_31_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_ULCF_CFG_32_31_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_ULCF_CFG_32_31_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_MON0_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_MON0_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_MON0_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_MON0_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_MON0_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_MON0_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_MON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_MON0_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_MON0_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_MON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_MON0_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_MON0_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_MON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_MON1_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_MON1_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_MON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_MON1_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_MON1_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_MON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_MON1_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_MON1_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_UL_SRC_MON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_UL_SRC_MON1_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_UL_SRC_MON1_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_SRC_DEBUG_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_SRC_DEBUG_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_SRC_DEBUG_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_SRC_DEBUG_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_SRC_DEBUG_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_SRC_DEBUG_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_SRC_DEBUG_MON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_SRC_DEBUG_MON0_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_SRC_DEBUG_MON0_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_SRC_DEBUG_MON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_SRC_DEBUG_MON0_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_SRC_DEBUG_MON0_0, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_6TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_6TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_6TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_6TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_6TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_6TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_6TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_6TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_6TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_6TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_6TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_6TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_6TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_6TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_6TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_6TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_6TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_6TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_6TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_6TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_6TH_DSN_DXI, value);
#endif
#ifdef MT6338_NLE_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_DL_NLE_CF_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_NLE_CF_H 0x%x = 0x%x\n",
			   MT6338_AFE_DL_NLE_CF_H, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_NLE_CFG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_NLE_CFG_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_NLE_CFG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_NLE_MON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_NLE_MON_H 0x%x = 0x%x\n",
			   MT6338_AFE_DL_NLE_MON_H, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_NLE_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_NLE_MON_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_NLE_MON_L, value);
#endif
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON1_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON1_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON1_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON1_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON2_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON2_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON2_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON2_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON3_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON3_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON4_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON4_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON4_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON4_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON5_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON5_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON5_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON5_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON5_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON5_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON6_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON6_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON6_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON6_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON6_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON6_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON7_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON7_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON7_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON7_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON7_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON7_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON8_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON8_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON8_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON8_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON8_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON8_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON9_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON9_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON9_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON9_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON9_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON9_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON10_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON10_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON10_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON10_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON10_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON10_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON11_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON11_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON11_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON11_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON11_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON11_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON12_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON12_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON12_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON12_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON12_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON12_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_MON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_MON0_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_MON0_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_MON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_MON0_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_MON0_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_MON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_MON1_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_MON1_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_MON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_MON1_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_MON1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADC_ASYNC_FIFO_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADC_ASYNC_FIFO_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_ADC_ASYNC_FIFO_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_AMIC_ARRAY_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_AMIC_ARRAY_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_AMIC_ARRAY_CFG, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON13 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON13, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON14, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON14 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON14, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON15_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON15_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON15_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON15_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON15_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON15_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON16_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON16_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON16_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON16_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON16_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON16_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON17_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON17_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON17_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON17_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON17_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON17_L, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON18_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON18_H 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON18_H, value);
	regmap_read(priv->regmap, MT6338_AFUNC_AUD_CON18_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFUNC_AUD_CON18_L 0x%x = 0x%x\n",
			   MT6338_AFUNC_AUD_CON18_L, value);
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_7TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_7TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_7TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_7TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_7TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_7TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_7TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_7TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_7TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_7TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_7TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_7TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_7TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_7TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_7TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_7TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_7TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_7TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_7TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_7TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_7TH_DSN_DXI, value);
#endif
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_CON1_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_CON1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_CON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_CON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_DEBUG_MON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_DEBUG_MON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_DEBUG_MON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_DEBUG_MON0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_DEBUG_MON0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON1_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON1_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON2_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON2_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON2_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON2_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON3_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON3_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON3_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON3_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_PREDIS_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_PREDIS_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_PREDIS_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_DCCOMP_CON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_TEST_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_TEST_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_TEST_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_TEST, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_TEST 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_TEST, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_DC_COMP_CFG0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_DC_COMP_CFG0_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_DC_COMP_CFG0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_DC_COMP_CFG0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_DC_COMP_CFG0_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_DC_COMP_CFG0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_DC_COMP_CFG0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_DC_COMP_CFG0_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_DC_COMP_CFG0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_DC_COMP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_DC_COMP_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_DC_COMP_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_DC_COMP_CFG1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_DC_COMP_CFG1_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_DC_COMP_CFG1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_DC_COMP_CFG1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_DC_COMP_CFG1_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_DC_COMP_CFG1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_DC_COMP_CFG1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_DC_COMP_CFG1_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_DC_COMP_CFG1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_DC_COMP_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_DC_COMP_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_DC_COMP_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_OUT_MON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_OUT_MON_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_OUT_MON_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_OUT_MON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_OUT_MON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_OUT_MON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_OUT_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_OUT_MON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_OUT_MON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_OUT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_OUT_MON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_OUT_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_LCH_MON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_LCH_MON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_LCH_MON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_LCH_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_LCH_MON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_LCH_MON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_LCH_MON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_LCH_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_RCH_MON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_RCH_MON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_RCH_MON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_RCH_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_RCH_MON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_RCH_MON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_RCH_MON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_RCH_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_DEBUG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_DEBUG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_DEBUG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SRC_DEBUG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SRC_DEBUG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DITHER_CON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_DITHER_CON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_DITHER_CON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_DITHER_CON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_DITHER_CON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_DITHER_CON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SDM_AUTO_RESET_CON, value);
#ifdef MT6338_XTALK_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H1R2L_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H1L2R_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON1_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON2_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON2_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON3_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON3_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON4_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON4_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON4_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2R2L_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2R2L_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2R2L_CON4, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON1_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON2_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON2_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON3_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON3_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON4_M 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON4_L 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON4_L, value);
	regmap_read(priv->regmap, MT6338_AFE_DL_XTALK_COMP_H2L2R_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_DL_XTALK_COMP_H2L2R_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_DL_XTALK_COMP_H2L2R_CON4, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_8TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_8TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_8TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_8TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_8TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_8TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_8TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_8TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_8TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_8TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_8TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_8TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_8TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_8TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_8TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_8TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_8TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_8TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_8TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_8TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_8TH_DSN_DXI, value);
#endif
#ifdef MT6338_NLE_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_NLE_CFG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_CFG_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_CFG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PRE_BUF_CFG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PRE_BUF_CFG_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PRE_BUF_CFG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PRE_BUF_CFG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PRE_BUF_CFG_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PRE_BUF_CFG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PRE_BUF_CFG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PRE_BUF_CFG_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PRE_BUF_CFG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PRE_BUF_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PRE_BUF_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PRE_BUF_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_CFG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_LCH_CFG_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_LCH_CFG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_CFG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_LCH_CFG_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_LCH_CFG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_CFG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_LCH_CFG_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_LCH_CFG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_LCH_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_LCH_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_ZCD_LCH_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_ZCD_LCH_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_ZCD_LCH_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_IMP_LCH_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_LCH_MON_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_LCH_MON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_LCH_MON 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_LCH_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_MON0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_LCH_MON1, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LCH_MON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LCH_MON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LCH_MON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LCH_MON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LCH_MON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LCH_MON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LCH_MON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LCH_MON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LCH_MON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LCH_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LCH_MON0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LCH_MON0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_CFG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_RCH_CFG_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_RCH_CFG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_CFG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_RCH_CFG_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_RCH_CFG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_CFG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_RCH_CFG_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_RCH_CFG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_RCH_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_RCH_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_ZCD_RCH_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_ZCD_RCH_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_ZCD_RCH_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_IMP_RCH_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_RCH_MON_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_RCH_MON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_PWR_DET_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_PWR_DET_RCH_MON 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_PWR_DET_RCH_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_MON0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_GAIN_ADJ_RCH_MON1, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_RCH_MON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_RCH_MON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_RCH_MON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_RCH_MON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_RCH_MON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_RCH_MON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_RCH_MON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_RCH_MON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_RCH_MON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_RCH_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_RCH_MON0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_RCH_MON0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G1, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G2, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G3, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G4, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G5, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G6, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_LCH_G7, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G0, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G1, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G2, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G3, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G4, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G5, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G6, value);
#endif
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_9TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_9TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_9TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_9TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_9TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_9TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_9TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_9TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_9TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_9TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_9TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_9TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_9TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_9TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_9TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_9TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_9TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_9TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_9TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_9TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_9TH_DSN_DXI, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_CON1_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_CON1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_CON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_CON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG_MON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG_MON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG_MON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG_MON0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG_MON0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_PREDIS_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_DCCOMP_CON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_TEST_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_TEST_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_TEST_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_TEST, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_TEST 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_TEST, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_DC_COMP_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_DEBUG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_LCH_MON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_LCH_MON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_LCH_MON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_LCH_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_LCH_MON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_LCH_MON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_LCH_MON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_LCH_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_RCH_MON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_RCH_MON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_RCH_MON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_RCH_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_RCH_MON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_RCH_MON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SRC_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SRC_RCH_MON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SRC_RCH_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_OUT_MON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_DITHER_CON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_DITHER_CON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_DITHER_CON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_DITHER_CON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_DITHER_CON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_DITHER_CON, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SDM_AUTO_RESET_CON, value);
#ifdef MT6338_XTALK_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H1R2L_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H1L2R_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON1_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON2_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON2_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON3_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON3_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON4_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON4_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON4_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2R2L_CON4, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON1_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON1_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON1_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON1_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON1_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON2_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON2_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON2_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON2_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON2_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON3_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON3_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON3_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON3_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON3_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON4_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON4_M 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON4_M, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON4_L 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON4_L, value);
	regmap_read(priv->regmap, MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_2ND_DL_XTALK_COMP_H2L2R_CON4, value);
#endif
#ifdef MT6338_NLE_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_LNGAIN_COMP_RCH_G7, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_D2A_DEBUG_H 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_D2A_DEBUG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_D2A_DEBUG_M 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_D2A_DEBUG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_D2A_DEBUG_L 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_D2A_DEBUG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_NLE_D2A_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_NLE_D2A_DEBUG 0x%x = 0x%x\n",
			   MT6338_AFE_NLE_D2A_DEBUG, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_10TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_10TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_10TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_10TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_10TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_10TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_10TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_10TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_10TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_10TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_10TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_10TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_10TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_10TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_10TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_10TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_10TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_10TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_10TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_10TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_10TH_DSN_DXI, value);
#endif
#ifdef MT6338_SCF_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP1_TAP2_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP3_TAP4_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP5_TAP6_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP7_TAP8_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP9_TAP10_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP11_TAP12_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP13_TAP14_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP15_TAP16_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP17_TAP18_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP19_TAP20_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP21_TAP22_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP23_TAP24_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP25_TAP26_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP27_TAP28_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP29_TAP30_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP31_TAP32_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP33_TAP34_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP35_TAP36_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP37_TAP38_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP39_TAP40_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP41_TAP42_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP43_TAP44_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP45_TAP46_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP47_TAP48_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP49_TAP50_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP51_TAP52_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP53_TAP54_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_HBF1_SCF1_TAP55_TAP56_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP57_TAP58_CONFIG, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_11TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_11TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_11TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_11TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_11TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_11TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_11TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_11TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_11TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_11TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_11TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_11TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_11TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_11TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_11TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_11TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_11TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_11TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_11TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_11TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_11TH_DSN_DXI, value);
#endif
#ifdef MT6338_SCF_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP59_TAP60_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP61_TAP62_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP63_TAP64_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP65_TAP66_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP67_TAP68_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP69_TAP70_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP71_TAP72_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP73_TAP74_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP75_TAP76_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP77_TAP78_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP79_TAP80_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP81_TAP82_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP83_TAP84_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP85_TAP86_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP87_TAP88_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP89_TAP90_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP91_TAP92_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP93_TAP94_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP95_TAP96_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP97_TAP98_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP99_TAP100_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP101_TAP102_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP103_TAP104_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP105_TAP106_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP107_TAP108_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP109_TAP110_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP111_TAP112_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP113_TAP114_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP115_TAP116_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP117_TAP118_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_12TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_12TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_12TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_12TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_12TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_12TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_12TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_12TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_12TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_12TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_12TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_12TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_12TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_12TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_12TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_12TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_12TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_12TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_12TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_12TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_12TH_DSN_DXI, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP119_TAP120_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP121_TAP122_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP123_TAP124_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP125_TAP126_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP127_TAP128_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP129_TAP130_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP131_TAP132_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP133_TAP134_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP135_TAP136_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP137_TAP138_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP139_TAP140_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP141_TAP142_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP143_TAP144_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP145_TAP146_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP147_TAP148_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP149_TAP150_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP151_TAP152_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP153_TAP154_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP155_TAP156_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP157_TAP158_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP159_TAP160_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP161_TAP162_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP163_TAP164_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP165_TAP166_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP167_TAP168_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP169_TAP170_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP171_TAP172_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP173_TAP174_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP175_TAP176_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP177_TAP178_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_13TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_13TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_13TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_13TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_13TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_13TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_13TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_13TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_13TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_13TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_13TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_13TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_13TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_13TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_13TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_13TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_13TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_13TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_13TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_13TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_13TH_DSN_DXI, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP179_TAP180_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP181_TAP182_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP183_TAP184_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP185_TAP186_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP187_TAP188_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP189_TAP190_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_DL_SCF1_TAP191_TAP192_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP179_TAP180_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP181_TAP182_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP183_TAP184_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP185_TAP186_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP187_TAP188_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP189_TAP190_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP191_TAP192_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_14TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_14TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_14TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_14TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_14TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_14TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_14TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_14TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_14TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_14TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_14TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_14TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_14TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_14TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_14TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_14TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_14TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_14TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_14TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_14TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_14TH_DSN_DXI, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP1_TAP2_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP3_TAP4_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP5_TAP6_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP7_TAP8_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP9_TAP10_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP11_TAP12_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP13_TAP14_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP15_TAP16_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP17_TAP18_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP19_TAP20_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP21_TAP22_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP23_TAP24_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP25_TAP26_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP27_TAP28_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP29_TAP30_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP31_TAP32_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP33_TAP34_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP35_TAP36_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP37_TAP38_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP39_TAP40_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP41_TAP42_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP43_TAP44_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP45_TAP46_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP47_TAP48_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP49_TAP50_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP51_TAP52_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP53_TAP54_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_HBF1_SCF1_TAP55_TAP56_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP57_TAP58_CONFIG, value);
#endif
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_15TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_15TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_15TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_15TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_15TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_15TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_15TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_15TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_15TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_15TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_15TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_15TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_15TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_15TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_15TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_15TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_15TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_15TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_15TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_15TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_15TH_DSN_DXI, value);
#ifdef MT6338_SCF_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP59_TAP60_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP61_TAP62_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP63_TAP64_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP65_TAP66_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP67_TAP68_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP69_TAP70_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP71_TAP72_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP73_TAP74_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP75_TAP76_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP77_TAP78_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP79_TAP80_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP81_TAP82_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP83_TAP84_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP85_TAP86_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP87_TAP88_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP89_TAP90_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP91_TAP92_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP93_TAP94_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP95_TAP96_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP97_TAP98_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP99_TAP100_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP101_TAP102_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP103_TAP104_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP105_TAP106_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP107_TAP108_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP109_TAP110_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP111_TAP112_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP113_TAP114_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP115_TAP116_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP117_TAP118_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_16TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_16TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_16TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_16TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_16TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_16TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_16TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_16TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_16TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_16TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_16TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_16TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_16TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_16TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_16TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_16TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_16TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_16TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_16TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_16TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_16TH_DSN_DXI, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP119_TAP120_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP121_TAP122_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP123_TAP124_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP125_TAP126_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP127_TAP128_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP129_TAP130_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP131_TAP132_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP133_TAP134_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP135_TAP136_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP137_TAP138_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP139_TAP140_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP141_TAP142_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP143_TAP144_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP145_TAP146_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP147_TAP148_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP149_TAP150_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP151_TAP152_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP153_TAP154_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP155_TAP156_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP157_TAP158_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP159_TAP160_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP161_TAP162_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP163_TAP164_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP165_TAP166_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP167_TAP168_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP169_TAP170_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP171_TAP172_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP173_TAP174_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP175_TAP176_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG_M, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG_M 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG_M, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG_L 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG_L, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_2ND_DL_SCF1_TAP177_TAP178_CONFIG, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_17TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_17TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_17TH_DSN_ID, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_17TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_17TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_17TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_17TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_17TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_17TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_17TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_17TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_17TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_17TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_17TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_17TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_17TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_17TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_17TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_17TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_17TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_17TH_DSN_DXI, value);
#endif
#ifdef MT6338_VOW_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_MON_SEL, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_MON_SEL 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_MON_SEL, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_MON_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_MON_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_MON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_MON_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_MON_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON0 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON1 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON1, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON2_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON2_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON2_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON2_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON3_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON3_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON3_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON3_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON4_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON4_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON4_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON4_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON5_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON5_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON5_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON5_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON5_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON5_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON6_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON6_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON6_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_CON6_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_CON6_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_CON6_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_WPTR_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_WPTR_MON_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_WPTR_MON_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_WPTR_MON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_WPTR_MON_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_WPTR_MON_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_RPTR_MON_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_RPTR_MON_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_RPTR_MON_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_RPTR_MON_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_RPTR_MON_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_RPTR_MON_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_RSV_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_RSV_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_RSV_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VAD_PBUF_RSV_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VAD_PBUF_RSV_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VAD_PBUF_RSV_H, value);
#endif
#ifdef MT6338_VOW_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON0 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON0, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON1 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON1, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON2 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON2, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON3 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON3, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON4 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON4, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON5 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON5, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON6 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON6, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON7 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON7, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON8 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON8, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON9 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON9, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON10 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON10, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TOP_CON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TOP_CON11 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TOP_CON11, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG2 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG2, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG3 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG3, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG4 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG4, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG5 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG5, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG6 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG6, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG7 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG7, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG8 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG8, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG9 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG9, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG10 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG10, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG11 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG11, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG12 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG12, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG13 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG13, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG14, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG14 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG14, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG15, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG15 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG15, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG16, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG16 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG16, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG17, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG17 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG17, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG18, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG18 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG18, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG19, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG19 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG19, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG20, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG20 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG20, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG21, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG21 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG21, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG22, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG22 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG22, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG23, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG23 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG23, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG24, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG24 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG24, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG25, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG25 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG25, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG26, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG26 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG26, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG27, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG27 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG27, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG28, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG28 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG28, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG29, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG29 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG29, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG30, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG30 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG30, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG31, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG31 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG31, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG32, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG32 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG32, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG33, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG33 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG33, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG34, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG34 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG34, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG35, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG35 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG35, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG36, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG36 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG36, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG37, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG37 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG37, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG38, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG38 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG38, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG39, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG39 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG39, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG40, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG40 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG40, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG41, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG41 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG41, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG42, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG42 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG42, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG43, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG43 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG43, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG44, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG44 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG44, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG45, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG45 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG45, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG46, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG46 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG46, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG47, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG47 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG47, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG48, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG48 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG48, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG49, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG49 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG49, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG50, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG50 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG50, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG51, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG51 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG51, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG52, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG52 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG52, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG53, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG53 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG53, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG54, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG54 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG54, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG55, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG55 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG55, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG56, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG56 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG56, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG57, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG57 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG57, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG58, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG58 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG58, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG59, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG59 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG59, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG60, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG60 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG60, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG61, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG61 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG61, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG62, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG62 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG62, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG63, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG63 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG63, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG64, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG64 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG64, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_CFG65, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_CFG65 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_CFG65, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TGEN_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TGEN_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TGEN_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TGEN_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TGEN_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TGEN_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TGEN_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TGEN_CFG2 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TGEN_CFG2, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TGEN_CFG3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TGEN_CFG3 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TGEN_CFG3, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TGEN_CFG4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TGEN_CFG4 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TGEN_CFG4, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TGEN_CFG5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TGEN_CFG5 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TGEN_CFG5, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TGEN_CFG6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TGEN_CFG6 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TGEN_CFG6, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_TGEN_CFG7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_TGEN_CFG7 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_TGEN_CFG7, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_HPF_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_HPF_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_HPF_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_HPF_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_HPF_CFG1 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_HPF_CFG1, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_HPF_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_HPF_CFG2 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_HPF_CFG2, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_HPF_CFG3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_HPF_CFG3 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_HPF_CFG3, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_HPF_CFG4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_HPF_CFG4 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_HPF_CFG4, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_HPF_CFG5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_HPF_CFG5 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_HPF_CFG5, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_HPF_CFG6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_HPF_CFG6 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_HPF_CFG6, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_HPF_CFG7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_HPF_CFG7 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_HPF_CFG7, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_INTR_CON, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_INTR_CON 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_INTR_CON, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_18TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_18TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_18TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_18TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_18TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_18TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_18TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_18TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_18TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_18TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_18TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_18TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_18TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_18TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_18TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_18TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_18TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_18TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_18TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_18TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_18TH_DSN_DXI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VOW_SRAM_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VOW_SRAM_L 0x%x = 0x%x\n",
			   MT6338_AUDIO_VOW_SRAM_L, value);
	regmap_read(priv->regmap, MT6338_AUDIO_VOW_SRAM_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_VOW_SRAM_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_VOW_SRAM_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_19TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_19TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_19TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_19TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_19TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_19TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_19TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_19TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_19TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_19TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_19TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_19TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_19TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_19TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_19TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_19TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_19TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_19TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_19TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_19TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_19TH_DSN_DXI, value);
#endif
#ifdef MTKAIFV4_SUPPORT
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_TX_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_TX_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_TX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_MTKAIFV4_TX_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_MTKAIFV4_TX_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_MTKAIFV4_TX_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_MTKAIFV4_TX_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_MTKAIFV4_TX_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_RX_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_RX_CFG1_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_MTKAIFV4_RX_CFG1_3, value);
	regmap_read(priv->regmap, MT6338_AFE_MTKAIFV4_RX_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_MTKAIFV4_RX_CFG 0x%x = 0x%x\n",
			   MT6338_AFE_MTKAIFV4_RX_CFG, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_MON0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_MON0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_MON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_MON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_MON0_H, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_MON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_MON1_0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_MON1_0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_MON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_MON1_1 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_MON1_1, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_MON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_MON1_2 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_MON1_2, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA_MTKAIFV4_MON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA_MTKAIFV4_MON1_3 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA_MTKAIFV4_MON1_3, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_MTKAIFV4_MON0 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_MTKAIFV4_MON0, value);
	regmap_read(priv->regmap, MT6338_AFE_ADDA6_MTKAIFV4_MON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_ADDA6_MTKAIFV4_MON0_H 0x%x = 0x%x\n",
			   MT6338_AFE_ADDA6_MTKAIFV4_MON0_H, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_20TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_20TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_20TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_20TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_20TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_20TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_20TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_20TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_20TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_20TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_20TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_20TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_20TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_20TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_20TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_20TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_20TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_20TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_20TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_20TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_20TH_DSN_DXI, value);
#endif
#ifndef	MTKAIFV4_SUPPORT
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON0_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON0_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON0_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON0_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON0_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON0_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON0_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON0_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON0_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON0_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON1_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON1_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON1_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON1_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON1_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON1_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON1_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON1_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON2_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON2_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON2_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON2_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON2_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON2_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON2_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON2_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON2_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON2_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON2_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON2_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON3_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON3_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON3_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON3_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON3_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON3_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON3_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON3_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON3_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON3_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON3_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON3_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON4_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON4_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON4_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON4_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON4_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON4_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON4_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON4_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON4_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON4_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON4_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON4_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON5_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON5_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON5_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON5_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON5_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON5_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON5_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON5_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON5_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON5_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON5_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON5_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON6_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON6_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON6_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON6_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON6_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON6_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON6_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON6_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON6_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON6_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON6_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON6_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON7_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON7_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON7_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON7_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON7_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON7_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON7_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON7_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON7_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON7_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON7_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON7_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON8_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON8_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON8_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON8_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON8_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON8_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON8_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON8_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON8_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_CON8_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_CON8_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_CON8_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON0_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON0_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON0_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON0_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON0_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON0_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON0_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON0_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON0_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON0_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON1_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON1_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON1_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON1_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON1_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON1_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON1_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON1_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON2_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON2_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON2_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON2_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON2_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON2_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON2_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON2_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON2_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON2_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON2_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON2_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON3_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON3_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON3_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON3_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON3_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON3_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON3_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON3_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON3_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON3_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON3_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON3_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON4_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON4_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON4_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON4_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON4_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON4_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON4_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON4_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON4_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON4_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON4_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON4_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON5_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON5_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON5_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON5_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON5_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON5_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON5_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON5_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON5_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON5_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON5_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON5_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON6_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON6_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON6_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON6_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON6_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON6_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON6_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON6_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON6_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON6_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON6_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON6_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON7_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON7_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON7_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON7_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON7_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON7_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON7_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON7_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON7_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON7_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON7_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON7_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON8_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON8_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON8_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON8_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON8_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON8_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON8_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON8_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON8_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON8_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON8_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON8_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON9_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON9_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON9_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON9_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON9_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON9_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON9_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON9_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON9_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_CON9_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_CON9_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_CON9_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_0_3_COWORK_CON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_0_3_COWORK_CON0_0 0x%x = 0x%x\n",
			   MT6338_ETDM_0_3_COWORK_CON0_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_0_3_COWORK_CON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_0_3_COWORK_CON0_1 0x%x = 0x%x\n",
			   MT6338_ETDM_0_3_COWORK_CON0_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_0_3_COWORK_CON0_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_0_3_COWORK_CON0_2 0x%x = 0x%x\n",
			   MT6338_ETDM_0_3_COWORK_CON0_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_0_3_COWORK_CON0_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_0_3_COWORK_CON0_3 0x%x = 0x%x\n",
			   MT6338_ETDM_0_3_COWORK_CON0_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_0_3_COWORK_CON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_0_3_COWORK_CON1_0 0x%x = 0x%x\n",
			   MT6338_ETDM_0_3_COWORK_CON1_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_0_3_COWORK_CON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_0_3_COWORK_CON1_1 0x%x = 0x%x\n",
			   MT6338_ETDM_0_3_COWORK_CON1_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_0_3_COWORK_CON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_0_3_COWORK_CON1_2 0x%x = 0x%x\n",
			   MT6338_ETDM_0_3_COWORK_CON1_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_0_3_COWORK_CON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_0_3_COWORK_CON1_3 0x%x = 0x%x\n",
			   MT6338_ETDM_0_3_COWORK_CON1_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN0_MON_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN0_MON_0 0x%x = 0x%x\n",
			   MT6338_ETDM_IN0_MON_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN1_MON_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN1_MON_1 0x%x = 0x%x\n",
			   MT6338_ETDM_IN1_MON_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN2_MON_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN2_MON_2 0x%x = 0x%x\n",
			   MT6338_ETDM_IN2_MON_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_IN3_MON_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_IN3_MON_3 0x%x = 0x%x\n",
			   MT6338_ETDM_IN3_MON_3, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT0_MON_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT0_MON_0 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT0_MON_0, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT1_MON_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT1_MON_1 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT1_MON_1, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT2_MON_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT2_MON_2 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT2_MON_2, value);
	regmap_read(priv->regmap, MT6338_ETDM_OUT3_MON_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ETDM_OUT3_MON_3 0x%x = 0x%x\n",
			   MT6338_ETDM_OUT3_MON_3, value);
#endif
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_21TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_21TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_21TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_21TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_21TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_21TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_21TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_21TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_21TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_21TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_21TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_21TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_21TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_21TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_21TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_21TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_21TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_21TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_21TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_21TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_21TH_DSN_DXI, value);
#ifdef MT6338_GAIN_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON0_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON0_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON0_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON0_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON1_3 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON1_3, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON1_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON1_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON1_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON1_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON1_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON1_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON2_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON2_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON2_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON2_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON2_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON2_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON2_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON2_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON2_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON3_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON3_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON3_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON3_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON3_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON3_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CON3_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CON3_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CON3_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CUR_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CUR_3 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CUR_3, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CUR_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CUR_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CUR_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CUR_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CUR_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CUR_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CUR_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CUR_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CUR_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CUR_PRE_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CUR_PRE_3 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CUR_PRE_3, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CUR_PRE_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CUR_PRE_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CUR_PRE_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CUR_PRE_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CUR_PRE_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CUR_PRE_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN1_CUR_PRE_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN1_CUR_PRE_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN1_CUR_PRE_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON0_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON0_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON0_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON0_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON0_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON0_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON1_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON1_3 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON1_3, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON1_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON1_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON1_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON1_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON1_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON1_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON1_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON1_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON1_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON2_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON2_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON2_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON2_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON2_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON2_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON2_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON2_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON2_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON3_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON3_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON3_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON3_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON3_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON3_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CON3_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CON3_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CON3_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CUR_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CUR_3 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CUR_3, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CUR_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CUR_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CUR_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CUR_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CUR_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CUR_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CUR_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CUR_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CUR_0, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CUR_PRE_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CUR_PRE_3 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CUR_PRE_3, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CUR_PRE_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CUR_PRE_2 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CUR_PRE_2, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CUR_PRE_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CUR_PRE_1 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CUR_PRE_1, value);
	regmap_read(priv->regmap, MT6338_AFE_GAIN2_CUR_PRE_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_GAIN2_CUR_PRE_0 0x%x = 0x%x\n",
			   MT6338_AFE_GAIN2_CUR_PRE_0, value);
#endif
#ifdef MT6338_OTHER_DEBUG
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_22TH_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_22TH_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_22TH_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_22TH_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_22TH_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_22TH_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_22TH_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_22TH_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_22TH_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_22TH_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_22TH_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_22TH_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_22TH_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_22TH_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_22TH_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_22TH_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_22TH_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_22TH_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDIO_DIG_22TH_DSN_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDIO_DIG_22TH_DSN_DXI 0x%x = 0x%x\n",
			   MT6338_AUDIO_DIG_22TH_DSN_DXI, value);
#endif
#ifdef MT6338_VOW_DEBUG
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON0 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON0, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON1 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON1, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON2 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON2, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON3 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON3, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON4 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON4, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON5 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON5, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON6 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON6, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON7 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON7, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON8 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON8, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON9 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON9, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON10 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON10, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON11 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON11, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON12 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON12, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON13 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON13, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON14, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON14 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON14, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON15, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON15 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON15, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON16, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON16 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON16, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON17, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON17 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON17, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON18, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON18 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON18, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON19, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON19 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON19, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON20, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON20 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON20, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON21, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON21 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON21, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON22, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON22 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON22, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON23, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON23 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON23, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON24, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON24 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON24, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON25, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON25 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON25, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON26, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON26 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON26, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON27, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON27 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON27, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON28, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON28 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON28, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON29, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON29 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON29, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON30, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON30 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON30, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON31, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON31 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON31, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON32, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON32 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON32, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON33, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON33 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON33, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON34, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON34 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON34, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON35, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON35 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON35, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON36, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON36 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON36, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON37, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON37 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON37, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON38, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON38 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON38, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON39, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON39 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON39, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON40, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON40 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON40, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON41, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON41 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON41, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON42, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON42 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON42, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON43, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON43 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON43, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON44, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON44 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON44, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON45, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON45 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON45, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON46, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON46 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON46, value);
	regmap_read(priv->regmap, MT6338_AFE_VOW_VAD_MON47, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AFE_VOW_VAD_MON47 0x%x = 0x%x\n",
			   MT6338_AFE_VOW_VAD_MON47, value);
#endif
	regmap_read(priv->regmap, MT6338_AUDENC_ANA_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_ANA_ID 0x%x = 0x%x\n",
			   MT6338_AUDENC_ANA_ID, value);
	regmap_read(priv->regmap, MT6338_AUDENC_DIG_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_DIG_ID 0x%x = 0x%x\n",
			   MT6338_AUDENC_DIG_ID, value);
	regmap_read(priv->regmap, MT6338_AUDENC_ANA_REV, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_ANA_REV 0x%x = 0x%x\n",
			   MT6338_AUDENC_ANA_REV, value);
	regmap_read(priv->regmap, MT6338_AUDENC_DIG_REV, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_DIG_REV 0x%x = 0x%x\n",
			   MT6338_AUDENC_DIG_REV, value);
	regmap_read(priv->regmap, MT6338_AUDENC_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_DBI 0x%x = 0x%x\n",
			   MT6338_AUDENC_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDENC_ESP, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_ESP 0x%x = 0x%x\n",
			   MT6338_AUDENC_ESP, value);
	regmap_read(priv->regmap, MT6338_AUDENC_FPI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_FPI 0x%x = 0x%x\n",
			   MT6338_AUDENC_FPI, value);
	regmap_read(priv->regmap, MT6338_AUDENC_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_DXI 0x%x = 0x%x\n",
			   MT6338_AUDENC_DXI, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON0 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON0, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON1 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON1, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON2 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON2, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON3 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON3, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON4 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON4, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON5 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON5, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON6 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON6, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON7 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON7, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON8 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON8, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON9 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON9, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON10 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON10, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON11 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON11, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON12 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON12, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON13 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON13, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON14, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON14 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON14, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON15, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON15 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON15, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON16, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON16 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON16, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON17, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON17 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON17, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON18, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON18 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON18, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON19, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON19 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON19, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON20, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON20 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON20, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON21, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON21 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON21, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON22, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON22 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON22, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON23, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON23 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON23, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON24, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON24 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON24, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON25, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON25 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON25, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON26, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON26 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON26, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON27, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON27 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON27, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON28, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON28 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON28, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON29, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON29 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON29, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON30, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON30 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON30, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON31, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON31 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON31, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON32, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON32 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON32, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON33, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON33 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON33, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON34, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON34 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON34, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON35, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON35 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON35, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON36, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON36 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON36, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON37, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON37 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON37, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON38, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON38 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON38, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON39, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON39 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON39, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON40, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON40 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON40, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON41, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON41 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON41, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON42, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON42 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON42, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON43, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON43 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON43, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON44, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON44 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON44, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON45, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON45 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON45, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON46, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON46 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON46, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON47, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON47 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON47, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON48, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON48 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON48, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON49, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON49 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON49, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON50, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON50 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON50, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON51, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON51 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON51, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON52, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON52 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON52, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON53, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON53 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON53, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON54, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON54 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON54, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON55, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON55 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON55, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON56, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON56 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON56, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON57, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON57 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON57, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON58, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON58 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON58, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON59, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON59 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON59, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON60, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON60 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON60, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON61, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON61 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON61, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON62, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON62 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON62, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON63, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON63 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON63, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON64, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON64 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON64, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON65, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON65 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON65, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON66, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON66 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON66, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON67, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON67 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON67, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON68, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON68 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON68, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON69, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON69 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON69, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON70, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON70 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON70, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON71, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON71 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON71, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON72, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON72 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON72, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON73, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON73 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON73, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON74, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON74 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON74, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON75, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON75 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON75, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON76, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON76 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON76, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON77, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON77 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON77, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON78, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON78 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON78, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON79, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON79 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON79, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON80, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON80 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON80, value);
	regmap_read(priv->regmap, MT6338_AUDENC_PMU_CON81, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_PMU_CON81 0x%x = 0x%x\n",
			   MT6338_AUDENC_PMU_CON81, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON0 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON0, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON1 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON1, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON2 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON2, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON3 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON3, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON4 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON4, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON5 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON5, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON6 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON6, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON7 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON7, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON8 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON8, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON9 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON9, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON10 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON10, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON11 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON11, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON12 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON12, value);
	regmap_read(priv->regmap, MT6338_AUDENC_2_PMU_CON13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_2_PMU_CON13 0x%x = 0x%x\n",
			   MT6338_AUDENC_2_PMU_CON13, value);
	regmap_read(priv->regmap, MT6338_AUDENC_ELR_NUM, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_ELR_NUM 0x%x = 0x%x\n",
			   MT6338_AUDENC_ELR_NUM, value);
	regmap_read(priv->regmap, MT6338_AUDENC_ELR_0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_ELR_0 0x%x = 0x%x\n",
			   MT6338_AUDENC_ELR_0, value);
	regmap_read(priv->regmap, MT6338_AUDENC_ELR_1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_ELR_1 0x%x = 0x%x\n",
			   MT6338_AUDENC_ELR_1, value);
	regmap_read(priv->regmap, MT6338_AUDENC_ELR_2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_ELR_2 0x%x = 0x%x\n",
			   MT6338_AUDENC_ELR_2, value);
	regmap_read(priv->regmap, MT6338_AUDENC_ELR_3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_ELR_3 0x%x = 0x%x\n",
			   MT6338_AUDENC_ELR_3, value);
	regmap_read(priv->regmap, MT6338_AUDENC_ELR_4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDENC_ELR_4 0x%x = 0x%x\n",
			   MT6338_AUDENC_ELR_4, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_ANA_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_ANA_ID 0x%x = 0x%x\n",
			   MT6338_AUDDEC_ANA_ID, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_DIG_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_DIG_ID 0x%x = 0x%x\n",
			   MT6338_AUDDEC_DIG_ID, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_ANA_REV, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_ANA_REV 0x%x = 0x%x\n",
			   MT6338_AUDDEC_ANA_REV, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_DIG_REV, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_DIG_REV 0x%x = 0x%x\n",
			   MT6338_AUDDEC_DIG_REV, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_DBI 0x%x = 0x%x\n",
			   MT6338_AUDDEC_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_ESP, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_ESP 0x%x = 0x%x\n",
			   MT6338_AUDDEC_ESP, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_FPI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_FPI 0x%x = 0x%x\n",
			   MT6338_AUDDEC_FPI, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_DXI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_DXI 0x%x = 0x%x\n",
			   MT6338_AUDDEC_DXI, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON0 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON0, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON1 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON1, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON2 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON2, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON3 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON3, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON4 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON4, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON5 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON5, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON6 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON6, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON7 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON7, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON8 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON8, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON9 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON9, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON10 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON10, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON11 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON11, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON12 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON12, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON13, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON13 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON13, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON14, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON14 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON14, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON15, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON15 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON15, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON16, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON16 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON16, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON17, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON17 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON17, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON18, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON18 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON18, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON19, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON19 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON19, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON20, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON20 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON20, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON21, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON21 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON21, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON22, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON22 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON22, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON23, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON23 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON23, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON24, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON24 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON24, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON25, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON25 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON25, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON26, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON26 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON26, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON27, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON27 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON27, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON28, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON28 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON28, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON29, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON29 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON29, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON30, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON30 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON30, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON31, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON31 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON31, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON32, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON32 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON32, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON33, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON33 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON33, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON34, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON34 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON34, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON35, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON35 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON35, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON36, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON36 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON36, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON37, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON37 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON37, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON38, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON38 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON38, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON39, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON39 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON39, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON40, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON40 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON40, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON41, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON41 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON41, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON42, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON42 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON42, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON43, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON43 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON43, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON44, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON44 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON44, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON45, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON45 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON45, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON46, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON46 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON46, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON47, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON47 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON47, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON48, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON48 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON48, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_PMU_CON49, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_PMU_CON49 0x%x = 0x%x\n",
			   MT6338_AUDDEC_PMU_CON49, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON0 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON0, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON1 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON1, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON2 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON2, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON3 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON3, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON4 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON4, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON5 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON5, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON6 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON6, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON7 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON7, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON8 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON8, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON9 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON9, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON10 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON10, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON11, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON11 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON11, value);
	regmap_read(priv->regmap, MT6338_AUDDEC_2_PMU_CON12, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDDEC_2_PMU_CON12 0x%x = 0x%x\n",
			   MT6338_AUDDEC_2_PMU_CON12, value);
	regmap_read(priv->regmap, MT6338_AUDZCD_DSN_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDZCD_DSN_ID 0x%x = 0x%x\n",
			   MT6338_AUDZCD_DSN_ID, value);
	regmap_read(priv->regmap, MT6338_AUDZCD_DSN_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDZCD_DSN_ID_H 0x%x = 0x%x\n",
			   MT6338_AUDZCD_DSN_ID_H, value);
	regmap_read(priv->regmap, MT6338_AUDZCD_DSN_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDZCD_DSN_REV0 0x%x = 0x%x\n",
			   MT6338_AUDZCD_DSN_REV0, value);
	regmap_read(priv->regmap, MT6338_AUDZCD_DSN_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDZCD_DSN_REV0_H 0x%x = 0x%x\n",
			   MT6338_AUDZCD_DSN_REV0_H, value);
	regmap_read(priv->regmap, MT6338_AUDZCD_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDZCD_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_AUDZCD_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_AUDZCD_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDZCD_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_AUDZCD_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_AUDZCD_DSN_FPI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_AUDZCD_DSN_FPI 0x%x = 0x%x\n",
			   MT6338_AUDZCD_DSN_FPI, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON0 0x%x = 0x%x\n",
			   MT6338_ZCD_CON0, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON1 0x%x = 0x%x\n",
			   MT6338_ZCD_CON1, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON1_H 0x%x = 0x%x\n",
			   MT6338_ZCD_CON1_H, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON2 0x%x = 0x%x\n",
			   MT6338_ZCD_CON2, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON2_H 0x%x = 0x%x\n",
			   MT6338_ZCD_CON2_H, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON3 0x%x = 0x%x\n",
			   MT6338_ZCD_CON3, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON4 0x%x = 0x%x\n",
			   MT6338_ZCD_CON4, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON4_H 0x%x = 0x%x\n",
			   MT6338_ZCD_CON4_H, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON5 0x%x = 0x%x\n",
			   MT6338_ZCD_CON5, value);
	regmap_read(priv->regmap, MT6338_ZCD_CON5_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ZCD_CON5_H 0x%x = 0x%x\n",
			   MT6338_ZCD_CON5_H, value);
#ifdef MT6338_ACCDET_DEBUG
	regmap_read(priv->regmap, MT6338_ACCDET_DSN_DIG_ID, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_DSN_DIG_ID 0x%x = 0x%x\n",
			   MT6338_ACCDET_DSN_DIG_ID, value);
	regmap_read(priv->regmap, MT6338_ACCDET_DSN_DIG_ID_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_DSN_DIG_ID_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_DSN_DIG_ID_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_DSN_DIG_REV0, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_DSN_DIG_REV0 0x%x = 0x%x\n",
			   MT6338_ACCDET_DSN_DIG_REV0, value);
	regmap_read(priv->regmap, MT6338_ACCDET_DSN_DIG_REV0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_DSN_DIG_REV0_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_DSN_DIG_REV0_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_DSN_DBI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_DSN_DBI 0x%x = 0x%x\n",
			   MT6338_ACCDET_DSN_DBI, value);
	regmap_read(priv->regmap, MT6338_ACCDET_DSN_DBI_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_DSN_DBI_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_DSN_DBI_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_DSN_FPI, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_DSN_FPI 0x%x = 0x%x\n",
			   MT6338_ACCDET_DSN_FPI, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON0_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON0_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON0_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON0_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON0_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON0_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON1_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON1_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON1_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON1_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON1_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON1_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON2_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON2_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON2_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON2_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON2_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON2_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON3_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON3_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON3_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON3_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON3_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON3_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON4_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON4_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON4_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON4_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON4_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON4_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON5_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON5_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON5_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON5_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON5_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON5_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON6 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON6, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON7_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON7_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON7_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON7_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON7_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON7_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON8_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON8_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON8_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON8_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON8_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON8_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON9_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON9_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON9_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON9_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON9_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON9_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON10_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON10_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON10_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON10_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON10_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON10_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON11_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON11_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON11_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON11_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON11_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON11_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON12_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON12_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON12_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON12_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON12_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON12_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON13_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON13_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON13_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON13_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON13_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON13_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON14_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON14_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON14_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON14_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON14_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON14_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON15, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON15 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON15, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON16_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON16_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON16_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON16_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON16_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON16_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON17, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON17 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON17, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON18_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON18_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON18_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON18_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON18_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON18_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON19_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON19_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON19_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON19_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON19_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON19_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON20_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON20_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON20_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON20_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON20_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON20_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON21_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON21_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON21_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON21_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON21_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON21_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON22_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON22_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON22_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON22_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON22_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON22_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON23_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON23_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON23_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON23_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON23_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON23_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON24, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON24 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON24, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON25_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON25_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON25_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON25_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON25_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON25_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON26_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON26_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON26_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON26_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON26_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON26_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON27_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON27_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON27_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON27_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON27_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON27_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON28_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON28_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON28_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON28_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON28_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON28_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON29_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON29_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON29_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON29_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON29_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON29_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON30_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON30_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON30_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON30_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON30_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON30_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON31_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON31_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON31_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON31_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON31_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON31_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON32_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON32_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON32_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON32_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON32_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON32_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON33_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON33_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON33_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON33_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON33_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON33_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON34_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON34_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON34_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON34_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON34_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON34_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON35_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON35_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON35_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON35_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON35_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON35_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON36, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON36 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON36, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON37, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON37 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON37, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON38_L, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON38_L 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON38_L, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON38_H, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON38_H 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON38_H, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON39, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON39 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON39, value);
	regmap_read(priv->regmap, MT6338_ACCDET_CON40, &value);
	n += scnprintf(buffer + n, size - n,
			   "MT6338_ACCDET_CON40 0x%x = 0x%x\n",
			   MT6338_ACCDET_CON40, value);
#endif
	return n;
}
static ssize_t mt6338_debugfs_read(struct file *file, char __user *buf,
				   size_t count, loff_t *pos)
{
	struct mt6338_priv *priv = file->private_data;
	const int size = 12288;
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0, ret = 0;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	n = mt6338_codec_read(priv, buffer, size);

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);
	return ret;
}

static ssize_t mt6338_codec_sysfs_write(struct file *filp, struct kobject *kobj,
					struct bin_attribute *bin_attr,
					char *buf, loff_t off, size_t count)
{
	struct mt6338_priv *priv = (struct mt6338_priv *)bin_attr->private;

	char input[MAX_DEBUG_WRITE_INPUT];
	char *temp, *command, *str_begin;
	char delim[] = " ,";

	if (!count) {
		dev_info(priv->dev, "%s(), count is 0, return directly\n",
			 __func__);
		goto exit;
	}

	if (count > MAX_DEBUG_WRITE_INPUT)
		count = MAX_DEBUG_WRITE_INPUT;

	memset((void *)input, 0, MAX_DEBUG_WRITE_INPUT);
	memcpy(input, buf, count);

	str_begin = kstrndup(input, MAX_DEBUG_WRITE_INPUT - 1,
			     GFP_KERNEL);
	if (!str_begin) {
		dev_info(priv->dev, "%s(), kstrdup fail\n", __func__);
		goto exit;
	}
	temp = str_begin;
	command = strsep(&temp, delim);
	dev_info(priv->dev, "%s(), temp=%s, command = %s\n",
		__func__, temp, command);

	if (strcmp("write_reg", command) == 0)
		codec_write_reg(priv, temp);

exit:
	return count;
}

static u32 copy_from_buffer_request(void *dest, size_t destsize, const void *src,
				    size_t srcsize, u32 offset, size_t request)
{
	/* if request == -1, offset == 0, copy full srcsize */
	if (offset + request > srcsize)
		request = srcsize - offset;

	/* if destsize == -1, don't check the request size */
	if (!dest || destsize < request) {
		pr_info("%s, buffer null or not enough space", __func__);
		return 0;
	}

	memcpy(dest, src + offset, request);

	return request;
}

/*
 * sysfs bin_attribute node
 */
static ssize_t mt6338_codec_sysfs_read(struct file *filep, struct kobject *kobj,
				       struct bin_attribute *attr,
				       char *buf, loff_t offset, size_t size)
{
	size_t read_size, ceil_size, page_mask;
	ssize_t ret;

	struct mt6338_priv *priv = (struct mt6338_priv *)attr->private;
	char *buffer = NULL; /* for reduce kernel stack */

	buffer = kzalloc(CODEC_SYS_DEBUG_SIZE, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	/* here read size may be different because of reg return may different */
	read_size = mt6338_codec_read(priv, buffer, CODEC_SYS_DEBUG_SIZE);
	page_mask = ~(PAGE_SIZE-1);
	ceil_size = (read_size&page_mask) + PAGE_SIZE;

	pr_info("%s buf[%p] offset = %lld size = %zu read_size[%zu]\n",
		    __func__, buf, offset, size, read_size);

	ret = copy_from_buffer_request(buf, -1, buffer, ceil_size, offset, size);
	if (ret < 0)
		ret = 0;

	kfree(buffer);

	return ret;
}

static ssize_t mt6338_debugfs_write(struct file *f, const char __user *buf,
				    size_t count, loff_t *offset)
{
	struct mt6338_priv *priv = f->private_data;
	char input[MAX_DEBUG_WRITE_INPUT];
	char *temp = NULL;
	char *command = NULL;
	char *str_begin = NULL;
	char delim[] = " ,";
	const struct command_function *cf;

	if (!count) {
		dev_info(priv->dev, "%s(), count is 0, return directly\n",
			 __func__);
		goto exit;
	}

	if (count > MAX_DEBUG_WRITE_INPUT)
		count = MAX_DEBUG_WRITE_INPUT;

	memset((void *)input, 0, MAX_DEBUG_WRITE_INPUT);

	if (copy_from_user(input, buf, count))
		dev_warn(priv->dev, "%s(), copy_from_user fail, count = %zu\n",
			 __func__, count);

	str_begin = kstrndup(input, MAX_DEBUG_WRITE_INPUT - 1,
			     GFP_KERNEL);
	if (!str_begin) {
		dev_info(priv->dev, "%s(), kstrdup fail\n", __func__);
		goto exit;
	}
	temp = str_begin;

	command = strsep(&temp, delim);

	dev_info(priv->dev, "%s(), command %s, content %s\n",
		 __func__, command, temp);

	for (cf = debug_cmds; cf->cmd; cf++) {
		if (strcmp(cf->cmd, command) == 0) {
			cf->fn(f, temp);
			break;
		}
	}

	kfree(str_begin);
exit:
	return count;
}

static const struct file_operations mt6338_debugfs_ops = {
	.open = mt6338_debugfs_open,
	.write = mt6338_debugfs_write,
	.read = mt6338_debugfs_read,
};

static int mt6338_parse_dt(struct mt6338_priv *priv)
{
	int ret, i;
	const int mux_num = 3;
	unsigned int mic_type_mux[3];
	struct device *dev = priv->dev;
	struct device_node *np;

	np = of_get_child_by_name(dev->parent->of_node, "mt6338_sound");
	if (!np)
		return -EINVAL;

	/* get mic type */
	ret = of_property_read_u32(np, "mediatek,dmic-mode",
				   &priv->dmic_one_wire_mode);
	if (ret) {
		dev_dbg(dev, "%s() failed to read dmic-mode, default 2 wire\n",
			 __func__);
		priv->dmic_one_wire_mode = 0;
	}
	ret = of_property_read_u32_array(np, "mediatek,mic-type",
					 mic_type_mux, mux_num);
	if (ret) {
		dev_dbg(dev, "%s() failed to read mic-type, default DCC\n",
			 __func__);
		priv->mux_select[MUX_MIC_TYPE_0] = MIC_TYPE_MUX_DCC;
		priv->mux_select[MUX_MIC_TYPE_1] = MIC_TYPE_MUX_DCC;
		priv->mux_select[MUX_MIC_TYPE_2] = MIC_TYPE_MUX_DCC;
		priv->mux_select[MUX_MIC_TYPE_3] = MIC_TYPE_MUX_DCC;
	} else {
		for (i = MUX_MIC_TYPE_0; i <= MUX_MIC_TYPE_2; ++i)
			priv->mux_select[i] = mic_type_mux[i];
	}

	ret = of_property_read_bool(dev->of_node, "vow_dmic_lp");
	if (ret) {
		priv->vow_dmic_lp = 1;
	} else {
		dev_info(dev, "%s() vow_dmic_lp node not exist, default off.\n",
			 __func__);
		priv->vow_dmic_lp = 0;
	}
#if IS_ENABLED(CONFIG_SND_SOC_MT6338_ACCDET)
	/* get auxadc channel */
	priv->hpofs_cal_auxadc = devm_iio_channel_get(dev,
						      "pmic_hpofs_cal");

	ret = PTR_ERR_OR_ZERO(priv->hpofs_cal_auxadc);
	if (ret) {
		if (ret != -EPROBE_DEFER)	//EPROBE_DEFER:517
			dev_err(dev,
				"%s() Get pmic_hpofs_cal iio ch failed (%d)\n",
				__func__, ret);
		else
			dev_err(dev,
				"%s() Get pmic_hpofs_cal iio ch failed (%d), will retry ...\n",
				__func__, ret);
	}
#endif
#if IS_ENABLED(CONFIG_MT6338_EFUSE)
	/* get pmic efuse handler */
	priv->hp_efuse = devm_nvmem_device_get(dev, "pmic-hp-efuse");
	ret = PTR_ERR_OR_ZERO(priv->hp_efuse);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "%s() Get efuse failed (%d)\n",
				__func__, ret);
		else
			dev_err(dev, "%s() Get efuse failed (%d), will retry ...\n",
				__func__, ret);
	}
#endif
	return 0;
}

static int mt6338_platform_driver_probe(struct platform_device *pdev)
{
	struct mt6338_priv *priv = NULL;
	int ret;

	dev_info(&pdev->dev, "%s(), dev name %s\n",
		 __func__, dev_name(&pdev->dev));

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, priv);
	priv->dev = &pdev->dev;

	/* get parent regmap */
	priv->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!priv->regmap) {
		dev_err(&pdev->dev, "Faled to get parent regmap\n");
		return -ENODEV;
	}

	/* create debugfs file */
	priv->debugfs = debugfs_create_file("mtksocanaaudio",
					    S_IFREG | 0444, NULL,
					    priv, &mt6338_debugfs_ops);

	ret = mt6338_parse_dt(priv);
	if (ret) {
		dev_warn(&pdev->dev,
			 "%s() fail to parse dts: %d\n", __func__, ret);
	}

	return devm_snd_soc_register_component(&pdev->dev,
					       &mt6338_soc_component_driver,
					       mt6338_dai_driver,
					       ARRAY_SIZE(mt6338_dai_driver));
}

static const struct of_device_id mt6338_of_match[] = {
	{.compatible = "mediatek,mt6338-sound",},
	{}
};
MODULE_DEVICE_TABLE(of, mt6338_of_match);

static struct platform_driver mt6338_platform_driver = {
	.driver = {
		.name = DEVICE_MT6338_NAME,
		.of_match_table = mt6338_of_match,
	},
	.probe = mt6338_platform_driver_probe,
};

module_platform_driver(mt6338_platform_driver)

/* Module information */
MODULE_DESCRIPTION("mt6338 ALSA SoC codec driver");
MODULE_AUTHOR("KaiChieh Chuang <kaichieh.chuang@mediatek.com>");
MODULE_AUTHOR("Eason Yen <eason.yen@mediatek.com>");
MODULE_AUTHOR("Shane Chien <shane.chien@mediatek.com>");
MODULE_AUTHOR("Ting-Fang Hou <Ting-Fang.Hou@mediatek.com>");
MODULE_LICENSE("GPL v2");
