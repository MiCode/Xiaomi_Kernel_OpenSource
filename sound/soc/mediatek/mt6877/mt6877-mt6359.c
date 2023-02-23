// SPDX-License-Identifier: GPL-2.0
/*
 *  mt6877-mt6359.c  --  mt6877 mt6359 ALSA SoC machine driver
 *
 *  Copyright (c) 2020 MediaTek Inc.
 *  Author: Eason Yen <eason.yen@mediatek.com>
 */

#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../common/mtk-afe-platform-driver.h"
#include "mt6877-afe-common.h"
#include "mt6877-afe-clk.h"
#include "mt6877-afe-gpio.h"
#include "../../codecs/mt6359.h"
#include "../common/mtk-sp-spk-amp.h"
#include "../../../../drivers/misc/mediatek/typec/tcpc/inc/tcpci_core.h"
#include "../../../../drivers/misc/mediatek/typec/tcpc/inc/tcpm.h"

#ifdef CONFIG_SND_SOC_AW87339
#include "aw87339.h"
#endif

/*
 * if need additional control for the ext spk amp that is connected
 * after Lineout Buffer / HP Buffer on the codec, put the control in
 * mt6877_mt6359_spk_amp_event()
 */
#define EXT_SPK_AMP_W_NAME "Ext_Speaker_Amp"

// #ifdef CONFIG_SND_SMARTPA_AW882XX
struct snd_soc_dai_link_component awinic_codecs[] = {
	{
		.of_node = NULL,
		.dai_name = "aw882xx-aif-7-34", /*以 I2C 总线 0x6，地址 0x34 举例*/
		.name = "aw882xx_smartpa.7-0034",
	},
	{
		.of_node = NULL,
		.dai_name = "aw882xx-aif-7-35", /*以 I2C 总线 0x6，地址 0x35 举例*/
		.name = "aw882xx_smartpa.7-0035",
	},
};
// #endif

static const char *const mt6877_spk_type_str[] = {MTK_SPK_NOT_SMARTPA_STR,
						  MTK_SPK_RICHTEK_RT5509_STR,
						  MTK_SPK_MEDIATEK_MT6660_STR,
						  MTK_SPK_NXP_TFA98XX_STR,
						  MTK_SPK_MEDIATEK_RT5512_STR,
						  MTK_SPK_AW_AW882XX_STR
						  };
static const char *const
	mt6877_spk_i2s_type_str[] = {MTK_SPK_I2S_0_STR,
				     MTK_SPK_I2S_1_STR,
				     MTK_SPK_I2S_2_STR,
				     MTK_SPK_I2S_3_STR,
				     MTK_SPK_I2S_5_STR,
				     MTK_SPK_I2S_6_STR,
				     MTK_SPK_I2S_7_STR,
				     MTK_SPK_I2S_8_STR,
				     MTK_SPK_I2S_9_STR,
				     MTK_SPK_TINYCONN_I2S_0_STR,
				     };

static const struct soc_enum mt6877_spk_type_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6877_spk_type_str),
			    mt6877_spk_type_str),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6877_spk_i2s_type_str),
			    mt6877_spk_i2s_type_str),
};

#ifdef CONFIG_SND_JACK_INPUT_DEV_RUBY
#define USB_3_5_UNSUPPORT 1
#endif

#ifdef USB_3_5_UNSUPPORT
struct snd_soc_jack g_usb_3_5_jack;
struct usb_priv *g_usbc_priv = NULL;
struct usb_priv {
	struct device *dev;
	struct notifier_block psy_nb;
	struct tcpc_device *tcpc_dev;
};

static int analog_usb_typec_event_changed(struct notifier_block *nb,
					unsigned long evt, void *ptr)
{
	int ret = 0;
	struct tcp_notify *noti = ptr;
	struct usb_priv *usbc_priv = container_of(nb, struct usb_priv, psy_nb);
	pr_info("%s: enter\n", __func__);
	if (NULL == noti) {
		pr_err("%s:data is NULL. \n", __func__);
		return 0;
	}

	if (!usbc_priv || (usbc_priv != g_usbc_priv))
		return -EINVAL;

	pr_info("%s:USB change event received, evt %d, expected %d, ole state %d, new state %d\n",
		__func__, evt, TCP_NOTIFY_TYPEC_STATE, noti->typec_state.old_state, noti->typec_state.new_state);
	switch (evt) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
			/* Audio Plug in */
			pr_info("%s: Audio Plug in\n", __func__);
			snd_soc_jack_report(&g_usb_3_5_jack, (SND_JACK_HEADSET | SND_JACK_VIDEOOUT),
			(SND_JACK_HEADSET | SND_JACK_VIDEOOUT));
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			/* Audio Plug out */
			pr_info("%s: Audio Plug out\n", __func__);
			snd_soc_jack_report(&g_usb_3_5_jack, 0, (SND_JACK_HEADSET | SND_JACK_VIDEOOUT));
		}
		break;
	}

	return ret;
}

static int analog_usb_typec_event_setup(struct platform_device *platform_device)
{
	int rc = 0;
	pr_info("%s: enter\n", __func__);
	if (NULL != g_usbc_priv) {
		pr_info("%s: had done! \n", __func__);
		return 0;
	}

	g_usbc_priv = devm_kzalloc(&platform_device->dev, sizeof(*g_usbc_priv),
				GFP_KERNEL);
	if (!g_usbc_priv)
		return -ENOMEM;

	g_usbc_priv->dev = &platform_device->dev;

	g_usbc_priv->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!g_usbc_priv->tcpc_dev) {
		rc = -EPROBE_DEFER;
		pr_err("%s get tcpc device type_c_port0 fail \n", __func__);
		goto err_data;
	}

	/* register tcpc_event */
	g_usbc_priv->psy_nb.notifier_call = analog_usb_typec_event_changed;
	g_usbc_priv->psy_nb.priority = 0;
	rc = register_tcp_dev_notifier(g_usbc_priv->tcpc_dev, &g_usbc_priv->psy_nb, TCP_NOTIFY_TYPE_USB);
	if (rc)
	{
		pr_err("%s: register_tcp_dev_notifier failed\n", __func__);
	}

	return 0;

err_data:
	devm_kfree(&platform_device->dev, g_usbc_priv);
	return rc;
}
#endif
static int mt6877_spk_type_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6877_spk_i2s_out_type_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_i2s_out_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6877_spk_i2s_in_type_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mtk_spk_get_i2s_in_type();

	pr_debug("%s() = %d\n", __func__, idx);
	ucontrol->value.integer.value[0] = idx;
	return 0;
}

static int mt6877_mt6359_spk_amp_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;

	dev_info(card->dev, "%s(), event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* spk amp on control */
#ifdef CONFIG_SND_SOC_AW87339
		aw87339_spk_enable_set(true);
#endif
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* spk amp off control */
#ifdef CONFIG_SND_SOC_AW87339
		aw87339_spk_enable_set(false);
#endif
		break;
	default:
		break;
	}

	return 0;
};

static const struct snd_soc_dapm_widget mt6877_mt6359_widgets[] = {
	SND_SOC_DAPM_SPK(EXT_SPK_AMP_W_NAME, mt6877_mt6359_spk_amp_event),
};

static const struct snd_soc_dapm_route mt6877_mt6359_routes[] = {
	{EXT_SPK_AMP_W_NAME, NULL, "LINEOUT L"},
	{EXT_SPK_AMP_W_NAME, NULL, "Headphone L Ext Spk Amp"},
	{EXT_SPK_AMP_W_NAME, NULL, "Headphone R Ext Spk Amp"},
};

static const struct snd_kcontrol_new mt6877_mt6359_controls[] = {
	SOC_DAPM_PIN_SWITCH(EXT_SPK_AMP_W_NAME),
	SOC_ENUM_EXT("MTK_SPK_TYPE_GET", mt6877_spk_type_enum[0],
		     mt6877_spk_type_get, NULL),
	SOC_ENUM_EXT("MTK_SPK_I2S_OUT_TYPE_GET", mt6877_spk_type_enum[1],
		     mt6877_spk_i2s_out_type_get, NULL),
	SOC_ENUM_EXT("MTK_SPK_I2S_IN_TYPE_GET", mt6877_spk_type_enum[1],
		     mt6877_spk_i2s_in_type_get, NULL),
};

/*
 * define mtk_spk_i2s_mck node in dts when need mclk,
 * BE i2s need assign snd_soc_ops = mt6877_mt6359_i2s_ops
 */
static int mt6877_mt6359_i2s_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 128;
	unsigned int mclk_fs = rate * mclk_fs_ratio;

	return snd_soc_dai_set_sysclk(rtd->cpu_dai,
				      0, mclk_fs, SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt6877_mt6359_i2s_ops = {
	.hw_params = mt6877_mt6359_i2s_hw_params,
};

static int mt6877_mt6359_mtkaif_calibration(struct snd_soc_pcm_runtime *rtd)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6877_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_component *codec_component =
		snd_soc_rtdcom_lookup(rtd, CODEC_MT6359_NAME);
	int phase;
	unsigned int monitor;
	int test_done_1, test_done_2, test_done_3;
	int miso0_need_calib, miso1_need_calib, miso2_need_calib;
	int cycle_1, cycle_2, cycle_3;
	int prev_cycle_1, prev_cycle_2, prev_cycle_3;
	int counter;
	int mtkaif_calib_ok;

	dev_info(afe->dev, "%s(), start\n", __func__);

	pm_runtime_get_sync(afe->dev);

	miso0_need_calib = mt6877_afe_gpio_is_prepared(MT6877_AFE_GPIO_DAT_MISO0_ON);
	miso1_need_calib = mt6877_afe_gpio_is_prepared(MT6877_AFE_GPIO_DAT_MISO1_ON);
	miso2_need_calib = mt6877_afe_gpio_is_prepared(MT6877_AFE_GPIO_DAT_MISO2_ON);

	mt6877_afe_gpio_request(afe, true, MT6877_DAI_ADDA, 1);
	mt6877_afe_gpio_request(afe, true, MT6877_DAI_ADDA, 0);
	mt6877_afe_gpio_request(afe, true, MT6877_DAI_ADDA_CH34, 1);
	mt6877_afe_gpio_request(afe, true, MT6877_DAI_ADDA_CH34, 0);

	mt6359_mtkaif_calibration_enable(codec_component);

	/* set clock protocol 2 */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP, 0xff, 0x38);
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP, 0xff, 0x39);

	/* set test type to synchronizer pulse */
	regmap_update_bits(afe_priv->topckgen,
			   CKSYS_AUD_TOP_CFG, 0xffff, 0x4);

	mtkaif_calib_ok = true;
	afe_priv->mtkaif_calibration_num_phase = 42;	/* mt6359: 0 ~ 42 */
	afe_priv->mtkaif_chosen_phase[0] = -1;
	afe_priv->mtkaif_chosen_phase[1] = -1;
	afe_priv->mtkaif_chosen_phase[2] = -1;

	for (phase = 0;
	     phase <= afe_priv->mtkaif_calibration_num_phase &&
	     mtkaif_calib_ok;
	     phase++) {
		mt6359_set_mtkaif_calibration_phase(codec_component,
						    phase, phase, phase);

		regmap_update_bits(afe_priv->topckgen,
				   CKSYS_AUD_TOP_CFG, 0x1, 0x1);

		test_done_1 = miso0_need_calib ? 0 : -1;
		test_done_2 = miso1_need_calib ? 0 : -1;
		test_done_3 = miso2_need_calib ? 0 : -1;
		cycle_1 = -1;
		cycle_2 = -1;
		cycle_3 = -1;
		counter = 0;
		while (test_done_1 == 0 ||
		       test_done_2 == 0 ||
		       test_done_3 == 0) {
			regmap_read(afe_priv->topckgen,
				    CKSYS_AUD_TOP_MON, &monitor);

			/* get test status */
			if (test_done_1 == 0)
				test_done_1 = (monitor >> 28) & 0x1;
			if (test_done_2 == 0)
				test_done_2 = (monitor >> 29) & 0x1;
			if (test_done_3 == 0)
				test_done_3 = (monitor >> 30) & 0x1;

			/* get delay cycle */
			if (test_done_1 == 1)
				cycle_1 = monitor & 0xf;

			if (test_done_2 == 1)
				cycle_2 = (monitor >> 4) & 0xf;

			if (test_done_3 == 1)
				cycle_3 = (monitor >> 8) & 0xf;

			/* handle if never test done */
			if (++counter > 10000) {
				dev_err(afe->dev, "%s(), test fail, cycle_1 %d, cycle_2 %d, cycle_3 %d, monitor 0x%x\n",
					__func__,
					cycle_1, cycle_2, cycle_3, monitor);
				mtkaif_calib_ok = false;
				break;
			}
		}

		if (phase == 0) {
			prev_cycle_1 = cycle_1;
			prev_cycle_2 = cycle_2;
			prev_cycle_3 = cycle_3;
		}

		if (miso0_need_calib && cycle_1 != prev_cycle_1 &&
		    afe_priv->mtkaif_chosen_phase[0] < 0) {
			afe_priv->mtkaif_chosen_phase[0] = phase - 1;
			afe_priv->mtkaif_phase_cycle[0] = prev_cycle_1;
		}

		if (miso1_need_calib && cycle_2 != prev_cycle_2 &&
		    afe_priv->mtkaif_chosen_phase[1] < 0) {
			afe_priv->mtkaif_chosen_phase[1] = phase - 1;
			afe_priv->mtkaif_phase_cycle[1] = prev_cycle_2;
		}

		if (miso2_need_calib && cycle_3 != prev_cycle_3 &&
		    afe_priv->mtkaif_chosen_phase[2] < 0) {
			afe_priv->mtkaif_chosen_phase[2] = phase - 1;
			afe_priv->mtkaif_phase_cycle[2] = prev_cycle_3;
		}

		regmap_update_bits(afe_priv->topckgen,
				   CKSYS_AUD_TOP_CFG, 0x1, 0x0);
	}

	mt6359_set_mtkaif_calibration_phase(codec_component,
		(afe_priv->mtkaif_chosen_phase[0] < 0) ?
		0 : afe_priv->mtkaif_chosen_phase[0],
		(afe_priv->mtkaif_chosen_phase[1] < 0) ?
		0 : afe_priv->mtkaif_chosen_phase[1],
		(afe_priv->mtkaif_chosen_phase[2] < 0) ?
		0 : afe_priv->mtkaif_chosen_phase[2]);

	/* disable rx fifo */
	regmap_update_bits(afe->regmap, AFE_AUD_PAD_TOP, 0xff, 0x38);

	mt6359_mtkaif_calibration_disable(codec_component);

	mt6877_afe_gpio_request(afe, false, MT6877_DAI_ADDA, 1);
	mt6877_afe_gpio_request(afe, false, MT6877_DAI_ADDA, 0);
	mt6877_afe_gpio_request(afe, false, MT6877_DAI_ADDA_CH34, 1);
	mt6877_afe_gpio_request(afe, false, MT6877_DAI_ADDA_CH34, 0);

	/* disable syncword if miso pin not prepared */
	if (!miso0_need_calib)
		regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIF_SYNCWORD_CFG,
				   RG_ADDA_MTKAIF_RX_SYNC_WORD1_DISABLE_MASK_SFT,
				   0x1 << RG_ADDA_MTKAIF_RX_SYNC_WORD1_DISABLE_SFT);
	if (!miso1_need_calib)
		regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIF_SYNCWORD_CFG,
				   RG_ADDA_MTKAIF_RX_SYNC_WORD2_DISABLE_MASK_SFT,
				   0x1 << RG_ADDA_MTKAIF_RX_SYNC_WORD2_DISABLE_SFT);
	/* miso2 need to sync word with miso1 */
	/* if only use miso2, disable syncword of miso1 */
	if (miso2_need_calib && !miso0_need_calib && !miso1_need_calib)
		regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIF_SYNCWORD_CFG,
				   RG_ADDA6_MTKAIF_RX_SYNC_WORD2_DISABLE_MASK_SFT,
				   0x1 << RG_ADDA6_MTKAIF_RX_SYNC_WORD2_DISABLE_SFT);
	pm_runtime_put(afe->dev);

	dev_info(afe->dev, "%s(), mtkaif_chosen_phase[0/1/2]:%d/%d/%d, miso_need_calib[%d/%d/%d]\n",
		 __func__,
		 afe_priv->mtkaif_chosen_phase[0],
		 afe_priv->mtkaif_chosen_phase[1],
		 afe_priv->mtkaif_chosen_phase[2],
		 miso0_need_calib, miso1_need_calib, miso2_need_calib);
#endif
	return 0;
}

static int mt6877_mt6359_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mt6359_codec_ops ops;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt6877_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dapm_context *dapm = &rtd->card->dapm;
	struct snd_soc_component *codec_component =
		snd_soc_rtdcom_lookup(rtd, CODEC_MT6359_NAME);

	ops.enable_dc_compensation = mt6877_enable_dc_compensation;
	ops.set_lch_dc_compensation = mt6877_set_lch_dc_compensation;
	ops.set_rch_dc_compensation = mt6877_set_rch_dc_compensation;
	ops.adda_dl_gain_control = mt6877_adda_dl_gain_control;
	mt6359_set_codec_ops(codec_component, &ops);

	/* set mtkaif protocol */
	mt6359_set_mtkaif_protocol(codec_component,
				   MT6359_MTKAIF_PROTOCOL_2_CLK_P2);
	afe_priv->mtkaif_protocol = MTKAIF_PROTOCOL_2_CLK_P2;

	/* mtkaif calibration */
	if (afe_priv->mtkaif_protocol == MTKAIF_PROTOCOL_2_CLK_P2)
		mt6877_mt6359_mtkaif_calibration(rtd);

	/* disable ext amp connection */
	snd_soc_dapm_disable_pin(dapm, EXT_SPK_AMP_W_NAME);

	return 0;
}

static int mt6877_i2s_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	dev_info(rtd->dev, "%s(), fix format to 32bit\n", __func__);

	/* fix BE i2s format to 32bit, clean param mask first */
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			     0, SNDRV_PCM_FORMAT_LAST);

	params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}

#ifdef CONFIG_MTK_VOW_SUPPORT
static const struct snd_pcm_hardware mt6877_mt6359_vow_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.period_bytes_min = 256,
	.period_bytes_max = 2 * 1024,
	.periods_min = 2,
	.periods_max = 4,
	.buffer_bytes_max = 2 * 2 * 1024,
};

static int mt6877_mt6359_vow_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct snd_soc_component *comp;
	struct snd_soc_rtdcom_list *rtdcom;

	dev_info(afe->dev, "%s(), start\n", __func__);
	snd_soc_set_runtime_hwparams(substream, &mt6877_mt6359_vow_hardware);

	mt6877_afe_gpio_request(afe, true, MT6877_DAI_VOW, 0);

	/* ASoC will call pm_runtime_get, but vow don't need */
	for_each_rtdcom(rtd, rtdcom) {
		comp = rtdcom->component;
		pm_runtime_put_autosuspend(comp->dev);
	}

	return 0;
}

static void mt6877_mt6359_vow_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct snd_soc_component *comp;
	struct snd_soc_rtdcom_list *rtdcom;

	dev_info(afe->dev, "%s(), end\n", __func__);
	mt6877_afe_gpio_request(afe, false, MT6877_DAI_VOW, 0);

	/* restore to fool ASoC */
	for_each_rtdcom(rtd, rtdcom) {
		comp = rtdcom->component;
		pm_runtime_get_sync(comp->dev);
	}
}

static const struct snd_soc_ops mt6877_mt6359_vow_ops = {
	.startup = mt6877_mt6359_vow_startup,
	.shutdown = mt6877_mt6359_vow_shutdown,
};
#endif  // #ifdef CONFIG_MTK_VOW_SUPPORT

static struct snd_soc_dai_link mt6877_mt6359_dai_links[] = {
	/* Front End DAI links */
	{
		.name = "Playback_1",
		.stream_name = "Playback_1",
		.cpu_dai_name = "DL1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Playback_12",
		.stream_name = "Playback_12",
		.cpu_dai_name = "DL12",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Playback_2",
		.stream_name = "Playback_2",
		.cpu_dai_name = "DL2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Playback_3",
		.stream_name = "Playback_3",
		.cpu_dai_name = "DL3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Playback_4",
		.stream_name = "Playback_4",
		.cpu_dai_name = "DL4",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Playback_5",
		.stream_name = "Playback_5",
		.cpu_dai_name = "DL5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Playback_6",
		.stream_name = "Playback_6",
		.cpu_dai_name = "DL6",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Playback_7",
		.stream_name = "Playback_7",
		.cpu_dai_name = "DL7",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Playback_8",
		.stream_name = "Playback_8",
		.cpu_dai_name = "DL8",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	{
		.name = "Playback_9",
		.stream_name = "Playback_9",
		.cpu_dai_name = "DL9",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
	},
	// 10
	{
		.name = "Capture_1",
		.stream_name = "Capture_1",
		.cpu_dai_name = "UL1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Capture_2",
		.stream_name = "Capture_2",
		.cpu_dai_name = "UL2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Capture_3",
		.stream_name = "Capture_3",
		.cpu_dai_name = "UL3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Capture_4",
		.stream_name = "Capture_4",
		.cpu_dai_name = "UL4",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Capture_5",
		.stream_name = "Capture_5",
		.cpu_dai_name = "UL5",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Capture_6",
		.stream_name = "Capture_6",
		.cpu_dai_name = "UL6",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Capture_7",
		.stream_name = "Capture_7",
		.cpu_dai_name = "UL7",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Capture_8",
		.stream_name = "Capture_8",
		.cpu_dai_name = "UL8",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Capture_Mono_1",
		.stream_name = "Capture_Mono_1",
		.cpu_dai_name = "UL_MONO_1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "Capture_Mono_2",
		.stream_name = "Capture_Mono_2",
		.cpu_dai_name = "UL_MONO_2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},
	// 20
	{
		.name = "Capture_Mono_3",
		.stream_name = "Capture_Mono_3",
		.cpu_dai_name = "UL_MONO_3",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
	},

	{
		.name = "Hostless_LPBK",
		.stream_name = "Hostless_LPBK",
		.cpu_dai_name = "Hostless LPBK DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_FM",
		.stream_name = "Hostless_FM",
		.cpu_dai_name = "Hostless FM DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_Speech",
		.stream_name = "Hostless_Speech",
		.cpu_dai_name = "Hostless Speech DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_Sph_Echo_Ref",
		.stream_name = "Hostless_Sph_Echo_Ref",
		.cpu_dai_name = "Hostless_Sph_Echo_Ref_DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_Spk_Init",
		.stream_name = "Hostless_Spk_Init",
		.cpu_dai_name = "Hostless_Spk_Init_DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_ADDA_DL_I2S_OUT",
		.stream_name = "Hostless_ADDA_DL_I2S_OUT",
		.cpu_dai_name = "Hostless_ADDA_DL_I2S_OUT DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_SRC_1",
		.stream_name = "Hostless_SRC_1",
		.cpu_dai_name = "Hostless_SRC_1_DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_SRC_Bargein",
		.stream_name = "Hostless_SRC_Bargein",
		.cpu_dai_name = "Hostless_SRC_Bargein_DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	/* Back End DAI links */
	{
		.name = "Primary Codec",
		.cpu_dai_name = "ADDA",
		.codec_dai_name = "mt6359-snd-codec-aif1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.init = mt6877_mt6359_init,
	},
	// 30
	{
		.name = "Primary Codec CH34",
		.cpu_dai_name = "ADDA_CH34",
		.codec_dai_name = "mt6359-snd-codec-aif2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "AP_DMIC",
		.cpu_dai_name = "AP_DMIC",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "AP_DMIC_CH34",
		.cpu_dai_name = "AP_DMIC_CH34",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "I2S3",
		.cpu_dai_name = "I2S3",
		//.codec_dai_name = "snd-soc-dummy-dai",
		// .codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6877_i2s_hw_params_fixup,
	},
	// 34
	{
		.name = "I2S0",
		.cpu_dai_name = "I2S0",
		// .codec_dai_name = "snd-soc-dummy-dai",
		// .codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6877_i2s_hw_params_fixup,
	},
	{
		.name = "I2S1",
		.cpu_dai_name = "I2S1",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6877_i2s_hw_params_fixup,
	},
	{
		.name = "I2S2",
		.cpu_dai_name = "I2S2",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6877_i2s_hw_params_fixup,
	},
	// 37
	{
		.name = "I2S5",
		.cpu_dai_name = "I2S5",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6877_i2s_hw_params_fixup,
	},
	{
		.name = "I2S6",
		.cpu_dai_name = "I2S6",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6877_i2s_hw_params_fixup,
	},
	{
		.name = "I2S7",
		.cpu_dai_name = "I2S7",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6877_i2s_hw_params_fixup,
	},
	{
		.name = "I2S8",
		.cpu_dai_name = "I2S8",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6877_i2s_hw_params_fixup,
	},
	{
		.name = "I2S9",
		.cpu_dai_name = "I2S9",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt6877_i2s_hw_params_fixup,
	},
	{
		.name = "HW Gain 1",
		.cpu_dai_name = "HW Gain 1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "HW Gain 2",
		.cpu_dai_name = "HW Gain 2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "HW_SRC_1",
		.cpu_dai_name = "HW_SRC_1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "HW_SRC_2",
		.cpu_dai_name = "HW_SRC_2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "CONNSYS_I2S",
		.cpu_dai_name = "CONNSYS_I2S",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "PCM 1",
		.cpu_dai_name = "PCM 1",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "PCM 2",
		.cpu_dai_name = "PCM 2",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	/* dummy BE for ul memif to record from dl memif */
	{
		.name = "Hostless_UL1",
		.cpu_dai_name = "Hostless_UL1 DAI",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_UL2",
		.cpu_dai_name = "Hostless_UL2 DAI",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_UL3",
		.cpu_dai_name = "Hostless_UL3 DAI",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_UL6",
		.cpu_dai_name = "Hostless_UL6 DAI",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_HW_Gain_AAudio",
		.stream_name = "Hostless_HW_Gain_AAudio",
		.cpu_dai_name = "Hostless HW Gain AAudio DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "Hostless_SRC_AAudio",
		.stream_name = "Hostless_SRC_AAudio",
		.cpu_dai_name = "Hostless SRC AAudio DAI",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	/* BTCVSD */
#ifdef CONFIG_SND_SOC_MTK_BTCVSD
	{
		.name = "BTCVSD",
		.stream_name = "BTCVSD",
		.cpu_dai_name   = "snd-soc-dummy-dai",
		.platform_name  = "18830000.mtk-btcvsd-snd",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
#endif
#if defined(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	{
		.name = "Offload_Playback",
		.stream_name = "Offload_Playback",
		.cpu_dai_name = "audio_task_offload_dai",
		.platform_name = "mt_soc_offload_common",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Playback_Voip",
		.stream_name = "DSP_Playback_Voip",
		.cpu_dai_name = "audio_task_voip_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Playback_Primary",
		.stream_name = "DSP_Playback_Primary",
		.cpu_dai_name = "audio_task_primary_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Playback_DeepBuf",
		.stream_name = "DSP_Playback_DeepBuf",
		.cpu_dai_name = "audio_task_deepbuf_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Playback_Playback",
		.stream_name = "DSP_Playback_Playback",
		.cpu_dai_name = "audio_task_Playback_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Capture_Ul1",
		.stream_name = "DSP_Capture_Ul1",
		.cpu_dai_name = "audio_task_capture_ul1_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Call_Final",
		.stream_name = "DSP_Call_Final",
		.cpu_dai_name = "audio_task_call_final_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Playback_Fast",
		.stream_name = "DSP_Playback_Fast",
		.cpu_dai_name = "audio_task_fast_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Playback_Ktv",
		.stream_name = "DSP_Playback_Ktv",
		.cpu_dai_name = "audio_task_ktv_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Capture_Raw",
		.stream_name = "DSP_Capture_Raw",
		.cpu_dai_name = "audio_task_capture_raw_dai",
		.platform_name = "snd_audio_dsp",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Playback_Fm_Adsp",
		.stream_name = "DSP_Playback_Fm_Adsp",
		.cpu_dai_name = "audio_task_fm_adsp_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
	{
		.name = "DSP_Playback_A2DP",
		.stream_name = "DSP_Playback_A2DP",
		.cpu_dai_name = "audio_task_a2dp_dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
#endif
#ifdef CONFIG_MTK_VOW_SUPPORT
	{
		.name = "VOW_Capture",
		.stream_name = "VOW_Capture",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.codec_dai_name = "mt6359-snd-codec-vow",
		.ignore_suspend = 1,
		.ops = &mt6877_mt6359_vow_ops,
	},
#endif  // #ifdef CONFIG_MTK_VOW_SUPPORT
#if defined(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
	{
		.name = "SCP_SPK_Playback",
		.stream_name = "SCP_SPK_Playback",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd_scp_spk",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
#endif
#if defined(CONFIG_MTK_ULTRASND_PROXIMITY)
	{
		.name = "SCP_ULTRA_Playback",
		.stream_name = "SCP_ULTRA_Playback",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd_scp_ultra",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
#endif
};

static struct snd_soc_card mt6877_mt6359_soc_card = {
	.name = "mt6877-mt6359",
	.owner = THIS_MODULE,
	.dai_link = mt6877_mt6359_dai_links,
	.num_links = ARRAY_SIZE(mt6877_mt6359_dai_links),

	.controls = mt6877_mt6359_controls,
	.num_controls = ARRAY_SIZE(mt6877_mt6359_controls),
	.dapm_widgets = mt6877_mt6359_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6877_mt6359_widgets),
	.dapm_routes = mt6877_mt6359_routes,
	.num_dapm_routes = ARRAY_SIZE(mt6877_mt6359_routes),
};

static int mt6877_mt6359_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt6877_mt6359_soc_card;
	struct device_node *platform_node, *codec_node, __attribute__((unused))*spk_node, *dsp_node;
	struct snd_soc_dai_link *spk_out_dai_link, *spk_iv_dai_link;
	int ret, i;
#ifdef USB_3_5_UNSUPPORT
	int status = 0;
#endif
	int spk_out_dai_link_idx, spk_iv_dai_link_idx;
	const char *name;

	dev_info(&pdev->dev, "%s(), ++\n", __func__);

	ret = mtk_spk_update_info(card, pdev,
				  &spk_out_dai_link_idx, &spk_iv_dai_link_idx,
				  &mt6877_mt6359_i2s_ops);
	if (ret) {
		dev_err(&pdev->dev, "%s(), mtk_spk_update_info error\n",
			__func__);
		return -EINVAL;
	}

	dev_info(&pdev->dev, "%s(), update spk dai\n", __func__);

	spk_out_dai_link = &mt6877_mt6359_dai_links[spk_out_dai_link_idx];
	spk_iv_dai_link = &mt6877_mt6359_dai_links[spk_iv_dai_link_idx];
	if (!spk_out_dai_link->codec_dai_name &&
	    !spk_iv_dai_link->codec_dai_name) {
#ifdef CONFIG_SND_SMARTPA_AW882XX
		dev_info(&pdev->dev, "%s(), CONFIG_SND_SMARTPA_AW882XX\n", __func__);
		spk_out_dai_link->codecs = awinic_codecs;
		spk_out_dai_link->num_codecs = ARRAY_SIZE(awinic_codecs);
		spk_iv_dai_link->codecs = awinic_codecs;
		spk_iv_dai_link->num_codecs = ARRAY_SIZE(awinic_codecs);
#else
		spk_node = of_get_child_by_name(pdev->dev.of_node,
					"mediatek,speaker-codec");
		if (!spk_node) {
			dev_err(&pdev->dev,
				"spk_codec of_get_child_by_name fail\n");
			return -EINVAL;
		}
		ret = snd_soc_of_get_dai_link_codecs(
				&pdev->dev, spk_node, spk_out_dai_link);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"i2s out get_dai_link_codecs fail\n");
			return -EINVAL;
		}
		ret = snd_soc_of_get_dai_link_codecs(
				&pdev->dev, spk_node, spk_iv_dai_link);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"i2s in get_dai_link_codecs fail\n");
			return -EINVAL;
		}
#endif
	}

	dev_info(&pdev->dev, "%s(), update platform dai\n", __func__);

	platform_node = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,platform", 0);
	if (!platform_node) {
		dev_err(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	dsp_node = of_parse_phandle(pdev->dev.of_node,
				    "mediatek,snd_audio_dsp", 0);
	if (!dsp_node)
		dev_info(&pdev->dev, "Property 'snd_audio_dsp' missing or invalid\n");

	for (i = 0; i < card->num_links; i++) {
		if (mt6877_mt6359_dai_links[i].platform_name)
			continue;
		/* no platform assign and with dsp playback node. */
		name = mt6877_mt6359_dai_links[i].name;
		if (!strncmp(name, "DSP", strlen("DSP")) &&
		    mt6877_mt6359_dai_links[i].platform_name == NULL) {
			mt6877_mt6359_dai_links[i].platform_of_node = dsp_node;
			continue;
		}
		mt6877_mt6359_dai_links[i].platform_of_node = platform_node;
	}

	dev_info(&pdev->dev, "%s(), update audio-codec dai\n", __func__);

	codec_node = of_parse_phandle(pdev->dev.of_node,
				      "mediatek,audio-codec", 0);
	if (!codec_node) {
		dev_err(&pdev->dev,
			"Property 'audio-codec' missing or invalid\n");
		return -EINVAL;
	}
	for (i = 0; i < card->num_links; i++) {
		if (mt6877_mt6359_dai_links[i].codec_name ||
		    i == spk_out_dai_link_idx ||
		    i == spk_iv_dai_link_idx)
			continue;
		if (mt6877_mt6359_dai_links[i].codecs)
			continue;
		mt6877_mt6359_dai_links[i].codec_of_node = codec_node;
	}

	card->dev = &pdev->dev;

	dev_info(&pdev->dev, "%s(), devm_snd_soc_register_card\n", __func__);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret)
		dev_err(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
#ifdef USB_3_5_UNSUPPORT
	pr_warn("%s: USB_3_5_UNSUPPORT\n", __func__);
	if (!ret) {
		status = analog_usb_typec_event_setup(pdev);
		if (status) {
			dev_err(&pdev->dev,"%s analog usb typeC event setup fail.ret:%d\n", __func__, ret);
		}
	}
#endif
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id mt6877_mt6359_dt_match[] = {
	{.compatible = "mediatek,mt6877-mt6359-sound",},
	{}
};
#endif

static const struct dev_pm_ops mt6877_mt6359_pm_ops = {
	.poweroff = snd_soc_poweroff,
	.restore = snd_soc_resume,
};

static struct platform_driver mt6877_mt6359_driver = {
	.driver = {
		.name = "mt6877-mt6359",
#ifdef CONFIG_OF
		.of_match_table = mt6877_mt6359_dt_match,
#endif
		.pm = &mt6877_mt6359_pm_ops,
	},
	.probe = mt6877_mt6359_dev_probe,
};

// module_platform_driver(mt6877_mt6359_driver);

static int __init mt6877_mt6359_driver_init(void)
{
	return platform_driver_register(&mt6877_mt6359_driver);
}

static void __exit mt6877_mt6359_driver_exit(void)
{
	platform_driver_unregister(&mt6877_mt6359_driver);
}

late_initcall(mt6877_mt6359_driver_init);
module_exit(mt6877_mt6359_driver_exit);

/* Module information */
MODULE_DESCRIPTION("MT6877 MT6359 ALSA SoC machine driver");
MODULE_AUTHOR("Eason Yen <eason.yen@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mt6877 mt6359 soc card");
