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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/qpnp/clkdiv.h>
#include <linux/io.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <asm/mach-types.h>
#include <mach/socinfo.h>
#include <qdsp6v2/msm-pcm-routing-v2.h>
#include <sound/q6afe-v2.h>
#include <linux/module.h>
#include <mach/gpiomux.h>
#include "../codecs/msm8x10-wcd.h"
#define DRV_NAME "msm8x10-asoc-wcd"
#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000

/* It takes about 13ms for Class-D PAs to ramp-up */
#define EXT_CLASS_D_EN_DELAY 13000
#define EXT_CLASS_D_DIS_DELAY 3000
#define EXT_CLASS_D_DELAY_DELTA 2000

#define CDC_EXT_CLK_RATE 9600000
#define WCD9XXX_MBHC_DEF_BUTTONS 8
#define WCD9XXX_MBHC_DEF_RLOADS 5

static int msm_btsco_rate = BTSCO_RATE_8KHZ;
static int msm_btsco_ch = 1;

static int msm_proxy_rx_ch = 2;
static struct platform_device *spdev;
static int ext_spk_amp_gpio = -1;

/* pointers for digital codec register mappings */
static void __iomem *pcbcr;
static void __iomem *prcgr;

static int msm_sec_mi2s_rx_ch = 1;
static int msm_pri_mi2s_tx_ch = 1;

static void *def_msm8x10_wcd_mbhc_cal(void);
static int msm8x10_enable_codec_ext_clk(struct snd_soc_codec *codec, int enable,
					bool dapm);
static struct wcd9xxx_mbhc_config mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = MBHC_MICBIAS1,
	.mclk_cb_fn = msm8x10_enable_codec_ext_clk,
	.mclk_rate = CDC_EXT_CLK_RATE,
	.gpio = 0,
	.gpio_irq = 0,
	.gpio_level_insert = 0,
	.detect_extn_cable = false,
	.insert_detect = true,
	.swap_gnd_mic = NULL,
	.use_int_rbias = false,
};

/*
 * There is limitation for the clock root selection from
 * either MI2S or DIG_CODEC.
 * If DIG_CODEC root can only provide 9.6MHz clock
 * to codec while MI2S only can provide
 * 12.288MHz.
 */
enum {
	DIG_CDC_CLK_SEL_DIG_CODEC,
	DIG_CDC_CLK_SEL_PRI_MI2S,
	DIG_CDC_CLK_SEL_SEC_MI2S,
};

static struct afe_clk_cfg mi2s_rx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_BOTH_VALID,
	0,
};

static struct afe_clk_cfg mi2s_tx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_BOTH_VALID,
	0,
};

static struct afe_digital_clk_cfg digital_cdc_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	9600000,
	5,  /* Digital Codec root */
	0,
};

static atomic_t mclk_rsc_ref;
static struct mutex cdc_mclk_mutex;

static int msm8x10_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);
static int msm_ext_spkramp_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event);
static void msm8x10_enable_ext_spk_power_amp(u32 on);

static const struct snd_soc_dapm_widget msm8x10_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msm8x10_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("Lineout amp", msm_ext_spkramp_event),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Secondary Mic", NULL),
};
static int msm8x10_ext_spk_power_amp_init(void)
{
	int ret = 0;

	ext_spk_amp_gpio = of_get_named_gpio(spdev->dev.of_node,
		"qcom,ext-spk-amp-gpio", 0);
	if (ext_spk_amp_gpio >= 0) {
		ret = gpio_request(ext_spk_amp_gpio, "ext_spk_amp_gpio");
		if (ret) {
			pr_err("%s: gpio_request failed for ext_spk_amp_gpio.\n",
				__func__);
			return -EINVAL;
		}
		gpio_direction_output(ext_spk_amp_gpio, 0);
	}
	return 0;
}

static int msm_ext_spkramp_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s()\n", __func__);

	if (ext_spk_amp_gpio >= 0) {
		if (SND_SOC_DAPM_EVENT_ON(event))
			msm8x10_enable_ext_spk_power_amp(1);
		else
			msm8x10_enable_ext_spk_power_amp(0);
	}
	return 0;

}

static void msm8x10_enable_ext_spk_power_amp(u32 on)
{
	if (on) {
		gpio_direction_output(ext_spk_amp_gpio, on);
		/*time takes enable the external power amplifier*/
		usleep_range(EXT_CLASS_D_EN_DELAY,
			     EXT_CLASS_D_EN_DELAY + EXT_CLASS_D_DELAY_DELTA);
	} else {
		gpio_direction_output(ext_spk_amp_gpio, on);
		/*time takes disable the external power amplifier*/
		usleep_range(EXT_CLASS_D_DIS_DELAY,
			     EXT_CLASS_D_DIS_DELAY + EXT_CLASS_D_DELAY_DELTA);
	}

	pr_debug("%s: %s external speaker PAs.\n", __func__,
			on ? "Enable" : "Disable");
}

static int msm_config_mclk(u16 port_id, struct afe_digital_clk_cfg *cfg)
{
	/* set the drive strength on the clock */
	msm_tlmm_misc_reg_write(TLMM_CDC_HDRV_CTL, 0x00);
	msm_tlmm_misc_reg_write(TLMM_CDC_HDRV_PULL_CTL, 0x0006db6d);

	iowrite32(0x1, pcbcr);
	/* Set the update bit to make the settings go through */
	iowrite32(0x1, prcgr);
	return 0;

}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}

static int msm_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s(): channel:%d\n", __func__, msm_pri_mi2s_tx_ch);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_sec_mi2s_rx_ch;

	return 0;
}

static int msm_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s(), channel:%d\n", __func__, msm_pri_mi2s_tx_ch);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_pri_mi2s_tx_ch;

	return 0;
}


static const char *const btsco_rate_text[] = {"8000", "16000"};
static const struct soc_enum msm_btsco_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
};
static const char *const sec_mi2s_rx_ch_text[] = {"One", "Two"};
static const char *const pri_mi2s_tx_ch_text[] = {"One", "Two"};

static int msm_btsco_rate_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_btsco_rate  = %d", __func__, msm_btsco_rate);

	ucontrol->value.integer.value[0] = msm_btsco_rate;
	return 0;
}

static int msm_btsco_rate_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 8000:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	case 16000:
		msm_btsco_rate = BTSCO_RATE_16KHZ;
		break;
	default:
		msm_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	}

	pr_debug("%s: msm_btsco_rate = %d\n", __func__, msm_btsco_rate);
	return 0;
}

static int msm_sec_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_sec_mi2s_rx_ch  = %d\n", __func__,
		 msm_sec_mi2s_rx_ch);
	ucontrol->value.integer.value[0] = msm_sec_mi2s_rx_ch - 1;
	return 0;
}

static int msm_sec_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_sec_mi2s_rx_ch = %d\n", __func__,
		 msm_sec_mi2s_rx_ch);
	return 1;
}

static int msm_pri_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_mi2s_tx_ch  = %d\n", __func__,
		 msm_pri_mi2s_tx_ch);
	ucontrol->value.integer.value[0] = msm_pri_mi2s_tx_ch - 1;
	return 0;
}

static int msm_pri_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_pri_mi2s_tx_ch = %d\n", __func__, msm_pri_mi2s_tx_ch);
	return 1;
}

static int msm_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm_btsco_rate;
	channels->min = channels->max = msm_btsco_ch;

	return 0;
}

static int msm_mi2s_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

static int mi2s_clk_ctl(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	if (enable) {
		digital_cdc_clk.clk_val = 9600000;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.clk_val2 = Q6AFE_LPASS_OSR_CLK_12_P288_MHZ;
			mi2s_rx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(AFE_PORT_ID_SECONDARY_MI2S_RX,
						  &mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.clk_val2 = Q6AFE_LPASS_OSR_CLK_12_P288_MHZ;
			mi2s_tx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
			ret = afe_set_lpass_clock(AFE_PORT_ID_PRIMARY_MI2S_RX,
						  &mi2s_tx_clk);
		} else
			pr_err("%s:Not valid substream.\n", __func__);

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);

	} else {
		digital_cdc_clk.clk_val = 0;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mi2s_rx_clk.clk_val2 = Q6AFE_LPASS_OSR_CLK_DISABLE;
			mi2s_rx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(AFE_PORT_ID_SECONDARY_MI2S_RX,
						  &mi2s_rx_clk);
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			mi2s_tx_clk.clk_val2 = Q6AFE_LPASS_OSR_CLK_DISABLE;
			mi2s_tx_clk.clk_val1 = Q6AFE_LPASS_IBIT_CLK_DISABLE;
			ret = afe_set_lpass_clock(AFE_PORT_ID_PRIMARY_MI2S_RX,
						  &mi2s_tx_clk);
		} else
			pr_err("%s:Not valid substream.\n", __func__);

		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock failed\n", __func__);

	}
	return ret;
}

static int msm8x10_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;

	mutex_lock(&cdc_mclk_mutex);

	pr_debug("%s: enable = %d  codec name %s enable %d mclk ref counter %d\n",
		   __func__, enable, codec->name, enable,
		   atomic_read(&mclk_rsc_ref));
	if (enable) {
		if (atomic_inc_return(&mclk_rsc_ref) == 1) {
			digital_cdc_clk.clk_val = 9600000;
			msm_config_mclk(AFE_PORT_ID_SECONDARY_MI2S_RX,
					&digital_cdc_clk);
			msm8x10_wcd_mclk_enable(codec, 1, dapm);
		}
	} else {
		if (atomic_dec_return(&mclk_rsc_ref) == 0) {
			digital_cdc_clk.clk_val = 0;
			msm8x10_wcd_mclk_enable(codec, 0, dapm);
			msm_config_mclk(AFE_PORT_ID_SECONDARY_MI2S_RX,
					&digital_cdc_clk);
		}
	}
	mutex_unlock(&cdc_mclk_mutex);
	return ret;
}

static int msm8x10_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm8x10_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm8x10_enable_codec_ext_clk(w->codec, 0, true);
	default:
		return -EINVAL;
	}
}

static void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	ret = mi2s_clk_ctl(substream, false);
	if (ret < 0)
		pr_err("%s:clock disable failed\n", __func__);
}

static int msm_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	ret = mi2s_clk_ctl(substream, true);

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_err("set fmt cpu dai failed\n");

	return ret;
}

static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, sec_mi2s_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, pri_mi2s_tx_ch_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm_btsco_enum[0],
		     msm_btsco_rate_get, msm_btsco_rate_put),
	SOC_ENUM_EXT("MI2S_RX Channels", msm_snd_enum[0],
			msm_sec_mi2s_rx_ch_get, msm_sec_mi2s_rx_ch_put),
	SOC_ENUM_EXT("MI2S_TX Channels", msm_snd_enum[1],
			msm_pri_mi2s_tx_ch_get, msm_pri_mi2s_tx_ch_put),
};

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{

	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	pr_debug("%s(),dev_name%s\n", __func__, dev_name(cpu_dai->dev));
	msm8x10_ext_spk_power_amp_init();

	mbhc_cfg.calibration = def_msm8x10_wcd_mbhc_cal();
	if (mbhc_cfg.calibration) {
		ret = msm8x10_wcd_hs_detect(codec, &mbhc_cfg);
		if (ret) {
			pr_err("%s: msm8x10_wcd_hs_detect failed\n", __func__);
			goto exit;
		}
	} else {
		ret = -ENOMEM;
		goto exit;
	}

	snd_soc_dapm_new_controls(dapm, msm8x10_dapm_widgets,
				ARRAY_SIZE(msm8x10_dapm_widgets));

	snd_soc_dapm_enable_pin(dapm, "Lineout amp");
	snd_soc_dapm_sync(dapm);

	ret = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (ret < 0)
		return ret;

exit:
	if (gpio_is_valid(ext_spk_amp_gpio))
		gpio_free(ext_spk_amp_gpio);

	return ret;
}

static void *def_msm8x10_wcd_mbhc_cal(void)
{
	void *msm8x10_wcd_cal;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	msm8x10_wcd_cal = kzalloc(WCD9XXX_MBHC_CAL_SIZE(
						WCD9XXX_MBHC_DEF_BUTTONS,
						WCD9XXX_MBHC_DEF_RLOADS),
					GFP_KERNEL);
	if (!msm8x10_wcd_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((WCD9XXX_MBHC_CAL_GENERAL_PTR(msm8x10_wcd_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 2);
	S(mbhc_navg, 128);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_DET_PTR(msm8x10_wcd_cal)->X) = (Y))
	S(mic_current, MSM8X10_WCD_PID_MIC_5_UA);
	S(hph_current, MSM8X10_WCD_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(msm8x10_wcd_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 1650);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_BTN_DET_PTR(msm8x10_wcd_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 5);
	S(mbhc_nsc, 10);
	S(n_btn_meas, 1);
	S(n_btn_con, 2);
	S(num_btn, WCD9XXX_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = WCD9XXX_MBHC_CAL_BTN_DET_PTR(msm8x10_wcd_cal);
	btn_low = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_V_BTN_LOW);
	btn_high = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg,
					       MBHC_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -50;
	btn_high[0] = 10;
	btn_low[1] = 11;
	btn_high[1] = 52;
	btn_low[2] = 53;
	btn_high[2] = 94;
	btn_low[3] = 95;
	btn_high[3] = 133;
	btn_low[4] = 134;
	btn_high[4] = 171;
	btn_low[5] = 172;
	btn_high[5] = 208;
	btn_low[6] = 209;
	btn_high[6] = 244;
	btn_low[7] = 245;
	btn_high[7] = 330;
	n_ready = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_READY);
	n_ready[0] = 80;
	n_ready[1] = 68;
	n_cic = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 14;

	return msm8x10_wcd_cal;
}

static int msm_proxy_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: msm_proxy_rx_ch =%d\n", __func__, msm_proxy_rx_ch);

	if (channels->max < 2)
		channels->min = channels->max = 2;
	channels->min = channels->max = msm_proxy_rx_ch;
	rate->min = rate->max = 48000;
	return 0;
}

static int msm_proxy_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = 48000;
	return 0;
}
static struct snd_soc_ops msm8x10_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.hw_params = msm_mi2s_snd_hw_params,
	.shutdown = msm_mi2s_snd_shutdown,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8x10_dai[] = {
	/* FrontEnd DAI Links */
	{/* hw:x,0 */
		.name = "MSM8X10 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{/* hw:x,1 */
		.name = "MSM8X10 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{/* hw:x,2 */
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
	},
	{/* hw:x,3 */
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{/* hw:x,4 */
		.name = "MSM8X10 LPA",
		.stream_name = "LPA",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-lpa",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PCM purpose */
	{/* hw:x,5 */
		.name = "Secondary MI2S RX Hostless",
		.stream_name = "Secondary MI2S_RX Hostless Playback",
		.cpu_dai_name = "SEC_MI2S_RX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* This dainlink has MI2S support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,6 */
		.name = "INT_FM Hostless",
		.stream_name = "INT_FM Hostless",
		.cpu_dai_name	= "INT_FM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,7 */
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6-dev.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{/* hw:x,8 */
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6-dev.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{/* hw:x,9 */
		.name = "MSM8X10 Compr",
		.stream_name = "COMPR",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compr-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{/* hw:x,10 */
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name   = "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,11 */
		.name = "Primary MI2S TX Hostless",
		.stream_name = "Primary MI2S_TX Hostless Capture",
		.cpu_dai_name = "PRI_MI2S_TX_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* This dainlink has MI2S support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,12 */
		.name = "MSM8x10 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	{/* hw:x,13 */
		.name = "Voice2",
		.stream_name = "Voice2",
		.cpu_dai_name   = "Voice2",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,14 */
		.name = "QCHAT",
		.stream_name = "QCHAT",
		.cpu_dai_name   = "QCHAT",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_QCHAT,
	},
	/* Backend I2S DAI Links */
	{
		.name = LPASS_BE_SEC_MI2S_RX,
		.stream_name = "Secondary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.1",
		.platform_name = "msm-pcm-routing",
		.codec_name     = MSM8X10_CODEC_NAME,
		.codec_dai_name = "msm8x10_wcd_i2s_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_rx_be_hw_params_fixup,
		.ops = &msm8x10_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = MSM8X10_CODEC_NAME,
		.codec_dai_name = "msm8x10_wcd_i2s_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = msm_tx_be_hw_params_fixup,
		.ops = &msm8x10_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_RX,
		.be_hw_params_fixup = msm_btsco_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_BT_SCO_TX,
		.stream_name = "Internal BT-SCO Capture",
		.cpu_dai_name = "msm-dai-q6-dev.12289",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_TX,
		.be_hw_params_fixup = msm_btsco_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_FM_RX,
		.stream_name = "Internal FM Playback",
		.cpu_dai_name = "msm-dai-q6-dev.12292",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_INT_FM_TX,
		.stream_name = "Internal FM Capture",
		.cpu_dai_name = "msm-dai-q6-dev.12293",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6-dev.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.be_hw_params_fixup = msm_proxy_rx_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6-dev.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm_proxy_tx_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32771",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.cpu_dai_name = "msm-dai-q6-dev.32773",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
};

struct snd_soc_card snd_soc_card_msm8x10 = {
	.name		= "msm8x10-snd-card",
	.dai_link	= msm8x10_dai,
	.num_links	= ARRAY_SIZE(msm8x10_dai),
};


static __devinit int msm8x10_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_msm8x10;
	int ret;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;

	ret = snd_soc_of_parse_audio_routing(card,
			"qcom,audio-routing");
	if (ret)
		goto err;

	mutex_init(&cdc_mclk_mutex);
	pcbcr = ioremap(MSM8X10_DINO_LPASS_DIGCODEC_CBCR, 4);
	if (!pcbcr) {
		ret = -ENOMEM;
		goto err1;
	}
	prcgr = ioremap(MSM8X10_DINO_LPASS_DIGCODEC_CMD_RCGR, 4);
	if (!prcgr) {
		ret = -ENOMEM;
		goto err1;
	}
	atomic_set(&mclk_rsc_ref, 0);
	mbhc_cfg.gpio_level_insert = of_property_read_bool(pdev->dev.of_node,
						"qcom,headset-jack-type-NC");
	mbhc_cfg.use_int_rbias = of_property_read_bool(pdev->dev.of_node,
						"qcom,mbhc-bias-internal");

	spdev = pdev;

	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER)
		goto err1;
	else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err1;
	}
	return 0;
err1:
	mutex_destroy(&cdc_mclk_mutex);
	if (pcbcr)
		iounmap(pcbcr);
	if (prcgr)
		iounmap(prcgr);
err:
	return ret;
}

static int __devexit msm8x10_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	if (gpio_is_valid(ext_spk_amp_gpio))
		gpio_free(ext_spk_amp_gpio);
	snd_soc_unregister_card(card);
	mutex_destroy(&cdc_mclk_mutex);

	iounmap(pcbcr);
	iounmap(prcgr);
	return 0;
}

static const struct of_device_id msm8x10_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8x10-audio-codec", },
	{},
};

static struct platform_driver msm8x10_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8x10_asoc_machine_of_match,
	},
	.probe = msm8x10_asoc_machine_probe,
	.remove = __devexit_p(msm8x10_asoc_machine_remove),
};
module_platform_driver(msm8x10_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8x10_asoc_machine_of_match);
