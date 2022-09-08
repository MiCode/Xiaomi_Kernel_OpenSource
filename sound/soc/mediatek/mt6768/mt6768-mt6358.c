// SPDX-License-Identifier: GPL-2.0
//
// mt6768-mt6358.c  --  mt6768 mt6358 ALSA SoC machine driver
//
// Copyright (c) 2022 MediaTek Inc.
// Author: Gang Feng <gang.feng@mediatek.com>

#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../common/mtk-afe-platform-driver.h"
#include "mt6768-afe-common.h"
#include "mt6768-afe-clk.h"
#include "mt6768-afe-gpio.h"
#include "../../codecs/mt6358.h"
#include "../common/mtk-sp-spk-amp.h"

#if IS_ENABLED(CONFIG_SND_SOC_MT6358_ACCDET)
      #include "../../codecs/mt6358-accdet.h"
#endif

/*
 * if need additional control for the ext spk amp that is connected
 * after Lineout Buffer / HP Buffer on the codec, put the control in
 * mt6768_mt6358_spk_amp_event()
 */
#define EXT_SPK_AMP_W_NAME "Ext_Speaker_Amp"

static const char *const mt6768_spk_type_str[] = {MTK_SPK_NOT_SMARTPA_STR,
						  MTK_SPK_RICHTEK_RT5509_STR,
						  MTK_SPK_MEDIATEK_MT6660_STR};
static const char *const mt6768_spk_i2s_type_str[] = {MTK_SPK_I2S_0_STR,
						      MTK_SPK_I2S_1_STR,
						      MTK_SPK_I2S_2_STR,
						      MTK_SPK_I2S_3_STR,
						      MTK_SPK_I2S_5_STR};

static const struct soc_enum mt6768_spk_type_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6768_spk_type_str),
			    mt6768_spk_type_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6768_spk_i2s_type_str),
			    mt6768_spk_i2s_type_str),
};

static int mt6768_spk_type_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6768_spk_i2s_out_type_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_i2s_out_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6768_spk_i2s_in_type_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_i2s_in_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6768_mt6358_spk_amp_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;

	dev_info(card->dev, "%s(), event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* spk amp on control */
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* spk amp off control */
		break;
	default:
		break;
	}

	return 0;
};

static const struct snd_soc_dapm_widget mt6768_mt6358_widgets[] = {
	SND_SOC_DAPM_SPK(EXT_SPK_AMP_W_NAME, mt6768_mt6358_spk_amp_event),
};

static const struct snd_soc_dapm_route mt6768_mt6358_routes[] = {
	{EXT_SPK_AMP_W_NAME, NULL, "LINEOUT L"},
	{EXT_SPK_AMP_W_NAME, NULL, "LINEOUT L HSSPK"},
	{EXT_SPK_AMP_W_NAME, NULL, "Headphone L Ext Spk Amp"},
	{EXT_SPK_AMP_W_NAME, NULL, "Headphone R Ext Spk Amp"},
};

static const struct snd_kcontrol_new mt6768_mt6358_controls[] = {
	SOC_DAPM_PIN_SWITCH(EXT_SPK_AMP_W_NAME),
	SOC_ENUM_EXT("MTK_SPK_TYPE_GET", mt6768_spk_type_enum[0],
		     mt6768_spk_type_get, NULL),
	SOC_ENUM_EXT("MTK_SPK_I2S_OUT_TYPE_GET", mt6768_spk_type_enum[1],
		     mt6768_spk_i2s_out_type_get, NULL),
	SOC_ENUM_EXT("MTK_SPK_I2S_IN_TYPE_GET", mt6768_spk_type_enum[1],
		     mt6768_spk_i2s_in_type_get, NULL),
};

/*
 * define mtk_spk_i2s_mck node in dts when need mclk,
 * BE i2s need assign snd_soc_ops = mt6768_mt6358_i2s_ops
 */
static int mt6768_mt6358_i2s_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 128;
	unsigned int mclk_fs = rate * mclk_fs_ratio;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	return snd_soc_dai_set_sysclk(cpu_dai,
				      0, mclk_fs, SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt6768_mt6358_i2s_ops = {
	.hw_params = mt6768_mt6358_i2s_hw_params,
};

static int mt6768_mt6358_mtkaif_calibration(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_component *codec_component =
		snd_soc_rtdcom_lookup(rtd, CODEC_MT6358_NAME);
	int phase = 0;
	unsigned int monitor = 0;
	int test_done_1, test_done_2 = 0;
	int cycle_1, cycle_2, prev_cycle_1, prev_cycle_2 = 0;
	int counter = 0;

	dev_info(afe->dev, "%s(), start\n", __func__);

	pm_runtime_get_sync(afe->dev);
	mt6768_afe_gpio_request(afe, true, MT6768_DAI_ADDA, 1);
	mt6768_afe_gpio_request(afe, true, MT6768_DAI_ADDA, 0);

	mt6358_mtkaif_calibration_enable(codec_component);

	/* set clock protocol 2 */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP, 0xff, 0x38);
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP, 0xff, 0x39);

	/* set test type to synchronizer pulse */
	regmap_update_bits(afe_priv->topckgen, CKSYS_AUD_TOP_CFG,
			   0xffff, 0x4);

	afe_priv->mtkaif_calibration_num_phase = RG_AUD_PAD_TOP_PHASE_MODE_MASK;
	afe_priv->mtkaif_calibration_ok = true;
	afe_priv->mtkaif_chosen_phase[0] = -1;
	afe_priv->mtkaif_chosen_phase[1] = -1;

	for (phase = 0;
	     phase <= afe_priv->mtkaif_calibration_num_phase &&
	     afe_priv->mtkaif_calibration_ok;
	     phase++) {
		mt6358_set_mtkaif_calibration_phase(codec_component,
						    phase, phase);

		regmap_update_bits(afe_priv->topckgen, CKSYS_AUD_TOP_CFG,
				   0x1, 0x1);

		test_done_1 = 0;
		test_done_2 = 0;
		cycle_1 = -1;
		cycle_2 = -1;
		counter = 0;
		while (test_done_1 == 0 || test_done_2 == 0) {
			regmap_read(afe_priv->topckgen, CKSYS_AUD_TOP_MON,
				    &monitor);

			test_done_1 = (monitor >> 28) & 0x1;
			test_done_2 = (monitor >> 29) & 0x1;
			if (test_done_1 == 1)
				cycle_1 = monitor & 0xf;

			if (test_done_2 == 1)
				cycle_2 = (monitor >> 4) & 0xf;

			/* handle if never test done */
			if (++counter > 10000) {
				dev_err(afe->dev, "%s(), test fail, cycle_1 %d, cycle_2 %d, monitor 0x%x\n",
					__func__,
					cycle_1, cycle_2, monitor);
				afe_priv->mtkaif_calibration_ok = false;
				break;
			}
		}

		if (phase == 0) {
			prev_cycle_1 = cycle_1;
			prev_cycle_2 = cycle_2;
		}

		if (cycle_1 != prev_cycle_1 &&
		    afe_priv->mtkaif_chosen_phase[0] < 0) {
			afe_priv->mtkaif_chosen_phase[0] = phase - 1;
			afe_priv->mtkaif_phase_cycle[0] = prev_cycle_1;
		}

		if (cycle_2 != prev_cycle_2 &&
		    afe_priv->mtkaif_chosen_phase[1] < 0) {
			afe_priv->mtkaif_chosen_phase[1] = phase - 1;
			afe_priv->mtkaif_phase_cycle[1] = prev_cycle_2;
		}

		regmap_update_bits(afe_priv->topckgen, CKSYS_AUD_TOP_CFG,
				   0x1, 0x0);

		if (afe_priv->mtkaif_chosen_phase[0] >= 0 &&
		    afe_priv->mtkaif_chosen_phase[1] >= 0)
			break;
	}

	if (!afe_priv->mtkaif_calibration_ok)
		mt6358_set_mtkaif_calibration_phase(codec_component,
						    0, 0);
	else
		mt6358_set_mtkaif_calibration_phase(codec_component,
			afe_priv->mtkaif_chosen_phase[0],
			afe_priv->mtkaif_chosen_phase[1]);

	/* disable rx fifo */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP, 0xff, 0x38);

	mt6358_mtkaif_calibration_disable(codec_component);

	mt6768_afe_gpio_request(afe, false, MT6768_DAI_ADDA, 1);
	mt6768_afe_gpio_request(afe, false, MT6768_DAI_ADDA, 0);
	pm_runtime_put(afe->dev);

	dev_info(afe->dev, "%s(), end, calibration ok %d\n",
		 __func__,
		 afe_priv->mtkaif_calibration_ok);

	return 0;
}

static int mt6768_mt6358_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mt6358_codec_ops ops;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dapm_context *dapm = &rtd->card->dapm;
	struct snd_soc_component *codec_component =
		snd_soc_rtdcom_lookup(rtd, CODEC_MT6358_NAME);
	ops.enable_dc_compensation = mt6768_enable_dc_compensation;
	ops.set_lch_dc_compensation = mt6768_set_lch_dc_compensation;
	ops.set_rch_dc_compensation = mt6768_set_rch_dc_compensation;
	ops.adda_dl_gain_control = mt6768_adda_dl_gain_control;
	mt6358_set_codec_ops(codec_component, &ops);

	/* set mtkaif protocol */
	mt6358_set_mtkaif_protocol(codec_component,
				   MT6358_MTKAIF_PROTOCOL_1);
	afe_priv->mtkaif_protocol = MT6358_MTKAIF_PROTOCOL_1;

	/* mtkaif calibration */
	if (afe_priv->mtkaif_protocol == MTKAIF_PROTOCOL_2_CLK_P2)
		mt6768_mt6358_mtkaif_calibration(rtd);

	/* disable ext amp connection */
	snd_soc_dapm_disable_pin(dapm, EXT_SPK_AMP_W_NAME);
#if IS_ENABLED(CONFIG_SND_SOC_MT6358_ACCDET)
	mt6358_accdet_init(codec_component, rtd->card);
#endif
	return 0;
}

static int mt6768_i2s_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	dev_info(rtd->dev, "%s(), fix format to 32bit\n", __func__);

	/* fix BE i2s format to 32bit, clean param mask first */
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			     0, SNDRV_PCM_FORMAT_LAST);

	params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}

#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
static const struct snd_pcm_hardware mt6768_mt6358_vow_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.period_bytes_min = 256,
	.period_bytes_max = 2 * 1024,
	.periods_min = 2,
	.periods_max = 4,
	.buffer_bytes_max = 2 * 2 * 1024,
};

static int mt6768_mt6358_vow_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct snd_soc_component *component = NULL;
	struct snd_soc_rtdcom_list *rtdcom = NULL;

	dev_info(afe->dev, "%s(), start\n", __func__);
	snd_soc_set_runtime_hwparams(substream, &mt6768_mt6358_vow_hardware);

	mt6768_afe_gpio_request(afe, true, MT6768_DAI_VOW, 0);

	/* ASoC will call pm_runtime_get, but vow don't need */
	for_each_rtdcom(rtd, rtdcom) {
		component = rtdcom->component;
		pm_runtime_put_autosuspend(component->dev);
	}
	return 0;
}

static void mt6768_mt6358_vow_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct snd_soc_component *component = NULL;
	struct snd_soc_rtdcom_list *rtdcom = NULL;

	dev_info(afe->dev, "%s(), end\n", __func__);
	mt6768_afe_gpio_request(afe, false, MT6768_DAI_VOW, 0);

	/* restore to fool ASoC */
	for_each_rtdcom(rtd, rtdcom) {
		component = rtdcom->component;
		pm_runtime_get_sync(component->dev);
	}
}

static const struct snd_soc_ops mt6768_mt6358_vow_ops = {
	.startup = mt6768_mt6358_vow_startup,
	.shutdown = mt6768_mt6358_vow_shutdown,
};
#endif  // #ifdef CONFIG_MTK_VOW_SUPPORT

/* FE */
SND_SOC_DAILINK_DEFS(playback1,
	DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback12,
	DAILINK_COMP_ARRAY(COMP_CPU("DL12")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback2,
	DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(playback3,
	DAILINK_COMP_ARRAY(COMP_CPU("DL3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture1,
	DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture2,
	DAILINK_COMP_ARRAY(COMP_CPU("UL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture3,
	DAILINK_COMP_ARRAY(COMP_CPU("UL3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture4,
	DAILINK_COMP_ARRAY(COMP_CPU("UL4")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture7,
	DAILINK_COMP_ARRAY(COMP_CPU("UL7")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(capture_mono_1,
	DAILINK_COMP_ARRAY(COMP_CPU("UL_MONO_1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* hostless */
SND_SOC_DAILINK_DEFS(hostless_lpbk,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless LPBK DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_fm,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless FM DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_speech,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless Speech DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_sph_echo_ref,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_Sph_Echo_Ref_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_spk_init,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_Spk_Init_DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_adda_dl_i2s_out,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_ADDA_DL_I2S_OUT DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* BE */
SND_SOC_DAILINK_DEFS(adda,
	DAILINK_COMP_ARRAY(COMP_CPU("ADDA")),
	DAILINK_COMP_ARRAY(COMP_CODEC(DEVICE_MT6358_NAME,
				      "mt6358-snd-codec-aif1")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2s0,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S0")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2s1,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2s2,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(i2s3,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_gain1,
	DAILINK_COMP_ARRAY(COMP_CPU("HW Gain 1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hw_gain2,
	DAILINK_COMP_ARRAY(COMP_CPU("HW Gain 2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(connsys_i2s,
	DAILINK_COMP_ARRAY(COMP_CPU("CONNSYS_I2S")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(pcm2,
	DAILINK_COMP_ARRAY(COMP_CPU("PCM 2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* hostless */
SND_SOC_DAILINK_DEFS(hostless_ul1,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL1 DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_ul2,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL2 DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_ul3,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL3 DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_ul4,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_UL4 DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
SND_SOC_DAILINK_DEFS(hostless_dsp_dl,
	DAILINK_COMP_ARRAY(COMP_CPU("Hostless_DSP_DL DAI")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
#if IS_ENABLED(CONFIG_SND_SOC_MTK_BTCVSD)
SND_SOC_DAILINK_DEFS(btcvsd,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("18050000.mtk-btcvsd-snd")));
#endif
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
SND_SOC_DAILINK_DEFS(vow,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_CODEC(DEVICE_MT6358_NAME,
						"mt6358-snd-codec-vow")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
#endif
#if (IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) \
&& IS_ENABLED(CONFIG_MTK_AUDIO_TUNNELING_SUPPORT))
SND_SOC_DAILINK_DEFS(dspoffload,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_offload_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("mt_soc_offload_common")));
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
SND_SOC_DAILINK_DEFS(dspvoip,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_voip_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd_audio_dsp")));
SND_SOC_DAILINK_DEFS(dspprimary,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_primary_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd_audio_dsp")));
SND_SOC_DAILINK_DEFS(dspdeepbuf,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_deepbuf_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd_audio_dsp")));
SND_SOC_DAILINK_DEFS(dspplayback,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_Playback_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd_audio_dsp")));
SND_SOC_DAILINK_DEFS(dspcapture1,
	DAILINK_COMP_ARRAY(COMP_CPU("audio_task_capture_ul1_dai")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd_audio_dsp")));
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
SND_SOC_DAILINK_DEFS(scpspk,
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("snd_scp_spk")));
#endif

static struct snd_soc_dai_link mt6768_mt6358_dai_links[] = {
	/* Front End DAI links */
	{
		.name = "Playback_1",
		.stream_name = "Playback_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback1),
	},
	{
		.name = "Playback_12",
		.stream_name = "Playback_12",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback12),
	},
	{
		.name = "Playback_2",
		.stream_name = "Playback_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback2),
	},
	{
		.name = "Playback_3",
		.stream_name = "Playback_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback3),
	},
	{
		.name = "Capture_1",
		.stream_name = "Capture_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture1),
	},
	{
		.name = "Capture_2",
		.stream_name = "Capture_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture2),
	},
	{
		.name = "Capture_3",
		.stream_name = "Capture_3",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture3),
	},
	{
		.name = "Capture_4",
		.stream_name = "Capture_4",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture4),
	},
	{
		.name = "Capture_7",
		.stream_name = "Capture_7",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture7),
	},
	{
		.name = "Capture_Mono_1",
		.stream_name = "Capture_Mono_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture_mono_1),
	},
	{
		.name = "Hostless_LPBK",
		.stream_name = "Hostless_LPBK",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_lpbk),
	},
	{
		.name = "Hostless_FM",
		.stream_name = "Hostless_FM",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_fm),
	},
	{
		.name = "Hostless_Speech",
		.stream_name = "Hostless_Speech",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_speech),
	},
	{
		.name = "Hostless_Sph_Echo_Ref",
		.stream_name = "Hostless_Sph_Echo_Ref",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_sph_echo_ref),
	},
	{
		.name = "Hostless_Spk_Init",
		.stream_name = "Hostless_Spk_Init",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_spk_init),
	},
	{
		.name = "Hostless_ADDA_DL_I2S_OUT",
		.stream_name = "Hostless_ADDA_DL_I2S_OUT",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_adda_dl_i2s_out),
	},
	/* Back End DAI links */
	{
		.name = "Primary Codec",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.init = mt6768_mt6358_init,
		SND_SOC_DAILINK_REG(adda),
	},
	{
		.name = "I2S3",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6768_mt6358_i2s_ops,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = mt6768_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2s3),
	},
	{
		.name = "I2S0",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6768_mt6358_i2s_ops,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = mt6768_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2s0),
	},
	{
		.name = "I2S1",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6768_mt6358_i2s_ops,
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6768_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2s1),
	},
	{
		.name = "I2S2",
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
			| SND_SOC_DAIFMT_GATED,
		.ops = &mt6768_mt6358_i2s_ops,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6768_i2s_hw_params_fixup,
		SND_SOC_DAILINK_REG(i2s2),
	},
	{
		.name = "HW Gain 1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_gain1),
	},
	{
		.name = "HW Gain 2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_gain2),
	},
	{
		.name = "CONNSYS_I2S",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(connsys_i2s),
	},
	{
		.name = "PCM 2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(pcm2),
	},
	/* dummy BE for ul memif to record from dl memif */
	{
		.name = "Hostless_UL1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_ul1),
	},
	{
		.name = "Hostless_UL2",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_ul2),
	},
	{
		.name = "Hostless_UL3",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_ul3),
	},
	{
		.name = "Hostless_UL4",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_ul4),
	},
	{
		.name = "Hostless_DSP_DL",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_dsp_dl),
	},
	/* BTCVSD */
#if IS_ENABLED(CONFIG_SND_SOC_MTK_BTCVSD)
	{
		.name = "BTCVSD",
		.stream_name = "BTCVSD",
		SND_SOC_DAILINK_REG(btcvsd),
	},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_AUDIO_TUNNELING_SUPPORT)
	{
		.name = "Offload_Playback",
		.stream_name = "Offload_Playback",
		SND_SOC_DAILINK_REG(dspoffload),
	},
#endif
	{
		.name = "DSP_Playback_Voip",
		.stream_name = "DSP_Playback_Voip",
		SND_SOC_DAILINK_REG(dspvoip),
	},
	{
		.name = "DSP_Playback_Primary",
		.stream_name = "DSP_Playback_Primary",
		SND_SOC_DAILINK_REG(dspprimary),
	},
	{
		.name = "DSP_Playback_DeepBuf",
		.stream_name = "DSP_Playback_DeepBuf",
		SND_SOC_DAILINK_REG(dspdeepbuf),
	},
	{
		.name = "DSP_Playback_Playback",
		.stream_name = "DSP_Playback_Playback",
		SND_SOC_DAILINK_REG(dspplayback),
	},
	{
		.name = "DSP_Capture_Ul1",
		.stream_name = "DSP_Capture_Ul1",
		SND_SOC_DAILINK_REG(dspcapture1),
	},
#endif
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
	{
		.name = "VOW_Capture",
		.stream_name = "VOW_Capture",
		.ignore_suspend = 1,
		.ops = &mt6768_mt6358_vow_ops,
		SND_SOC_DAILINK_REG(vow),
	},
#endif  // #ifdef CONFIG_MTK_VOW_SUPPORT
#if IS_ENABLED(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
	{
		.name = "SCP_SPK_Playback",
		.stream_name = "SCP_SPK_Playback",
		SND_SOC_DAILINK_REG(scpspk),
	},
#endif
};

static struct snd_soc_card mt6768_mt6358_soc_card = {
	.name = "mt6768-mt6358",
	.owner = THIS_MODULE,
	.dai_link = mt6768_mt6358_dai_links,
	.num_links = ARRAY_SIZE(mt6768_mt6358_dai_links),

	.controls = mt6768_mt6358_controls,
	.num_controls = ARRAY_SIZE(mt6768_mt6358_controls),
	.dapm_widgets = mt6768_mt6358_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6768_mt6358_widgets),
	.dapm_routes = mt6768_mt6358_routes,
	.num_dapm_routes = ARRAY_SIZE(mt6768_mt6358_routes),
};

static int mt6768_mt6358_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt6768_mt6358_soc_card;
	struct device_node *platform_node, *spk_node = NULL;
	struct snd_soc_dai_link *dai_link = NULL;
	int ret = 0;
	int i = 0;

	dev_info(&pdev->dev, "%s()\n", __func__);

	/* update speaker type */
	ret = mtk_spk_update_info(card, pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s(), mtk_spk_update_info error\n",
			__func__);
		return -EINVAL;
	}

	platform_node = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,platform", 0);
	if (!platform_node) {
		dev_err(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	/* get speaker codec node */
	spk_node = of_get_child_by_name(pdev->dev.of_node,
					"mediatek,speaker-codec");
	if (!spk_node) {
		dev_err(&pdev->dev,
			"spk_node of_get_child_by_name fail\n");
		//return -EINVAL;
	}

	for_each_card_prelinks(card, i, dai_link) {
		if (!dai_link->platforms->name)
			dai_link->platforms->of_node = platform_node;

		if (!strcmp(dai_link->name, "Speaker Codec")) {
			ret = snd_soc_of_get_dai_link_codecs(
						&pdev->dev, spk_node, dai_link);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"Speaker Codec get_dai_link fail: %d\n", ret);
				return -EINVAL;
			}
		} else if (!strcmp(dai_link->name, "Speaker Codec Ref")) {
			ret = snd_soc_of_get_dai_link_codecs(
						&pdev->dev, spk_node, dai_link);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"Speaker Codec Ref get_dai_link fail: %d\n", ret);
				return -EINVAL;
			}
		}
	}

	card->dev = &pdev->dev;

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret)
		dev_err(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
	else
		dev_err(&pdev->dev, "%s snd_soc_register_card pass %d\n",
				__func__, ret);
	return ret;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mt6768_mt6358_dt_match[] = {
	{.compatible = "mediatek,mt6768-mt6358-sound",},
	{}
};
#endif

static const struct dev_pm_ops mt6768_mt6358_pm_ops = {
	.poweroff = snd_soc_poweroff,
	.restore = snd_soc_resume,
};

static struct platform_driver mt6768_mt6358_driver = {
	.driver = {
		.name = "mt6768-mt6358",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = mt6768_mt6358_dt_match,
#endif
		.pm = &mt6768_mt6358_pm_ops,
	},
	.probe = mt6768_mt6358_dev_probe,
};

module_platform_driver(mt6768_mt6358_driver);

/* Module information */
MODULE_DESCRIPTION("MT6768 MT6358 ALSA SoC machine driver");
MODULE_AUTHOR("Gang Feng <gang.feng@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mt6768 mt6358 soc card");
