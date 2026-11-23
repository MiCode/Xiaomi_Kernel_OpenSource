// SPDX-License-Identifier: GPL-2.0
//
// cs40l26.c -- ALSA SoC Audio driver for Cirrus Logic Haptic Device: CS40L26
//
// Copyright 2022 Cirrus Logic. Inc.
#define DEBUG

#define CUSTOMIZED_I2S
#define REG_DBG
#define PREVENT_WAKEUP_BY_DAI_OPS
#define I2S_ATTENUATION_LINEARIZE

#include <linux/mfd/cs40l26.h>

static int cs40l26_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt);
static int cs40l26_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
		unsigned int rx_mask, int slots, int slot_width);

static const struct cs40l26_pll_sysclk_config cs40l26_pll_sysclk[] = {
	{CS40L26_PLL_CLK_FRQ_32768, CS40L26_PLL_CLK_CFG_32768},
	{CS40L26_PLL_CLK_FRQ_1536000, CS40L26_PLL_CLK_CFG_1536000},
	{CS40L26_PLL_CLK_FRQ_3072000, CS40L26_PLL_CLK_CFG_3072000},
	{CS40L26_PLL_CLK_FRQ_6144000, CS40L26_PLL_CLK_CFG_6144000},
	{CS40L26_PLL_CLK_FRQ_9600000, CS40L26_PLL_CLK_CFG_9600000},
	{CS40L26_PLL_CLK_FRQ_12288000, CS40L26_PLL_CLK_CFG_12288000},
};

static int cs40l26_get_clk_config(u32 freq, u8 *clk_cfg)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs40l26_pll_sysclk); i++) {
		if (cs40l26_pll_sysclk[i].freq == freq) {
			*clk_cfg = cs40l26_pll_sysclk[i].clk_cfg;
			return 0;
		}
	}

	return -EINVAL;
}

static int cs40l26_swap_ext_clk(struct cs40l26_codec *codec, u8 clk_src)
{
	struct regmap *regmap = codec->regmap;
	struct device *dev = codec->dev;
	u8 clk_cfg, clk_sel;
	int ret;

	switch (clk_src) {
	case CS40L26_PLL_REFCLK_BCLK:
		clk_sel = CS40L26_PLL_CLK_SEL_BCLK;
		ret = cs40l26_get_clk_config(codec->sysclk_rate, &clk_cfg);
		break;
	case CS40L26_PLL_REFCLK_MCLK:
		clk_sel = CS40L26_PLL_CLK_SEL_MCLK;
		ret = cs40l26_get_clk_config(CS40L26_PLL_CLK_FRQ_32768, &clk_cfg);
		break;
	case CS40L26_PLL_REFCLK_FSYNC:
		ret = -EPERM;
		break;
	default:
		ret = -EINVAL;
	}

	if (ret) {
		dev_err(dev, "Failed to get clock configuration\n");
		return cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_ASP, __func__);
	}

	ret = cs40l26_set_pll_loop(codec->core, CS40L26_PLL_REFCLK_SET_OPEN_LOOP);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, CS40L26_REFCLK_INPUT, CS40L26_PLL_REFCLK_FREQ_MASK |
			CS40L26_PLL_REFCLK_SEL_MASK, (clk_cfg << CS40L26_PLL_REFCLK_FREQ_SHIFT) |
			clk_sel);
	if (ret) {
		dev_err(dev, "Failed to update REFCLK input\n");
		return cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_CP, __func__);
	}

	return cs40l26_set_pll_loop(codec->core, CS40L26_PLL_REFCLK_SET_CLOSED_LOOP);
}

static int cs40l26_clk_en(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol, int event)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_dapm_to_component(w->dapm));
	struct cs40l26_private *cs40l26 = codec->core;
	struct device *dev = cs40l26->dev;
	int ret;

	dev_dbg(dev, "%s: %s\n", __func__, event == SND_SOC_DAPM_POST_PMU ? "PMU" : "PMD");

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		mutex_lock(&cs40l26->lock);
		cs40l26_vibe_state_update(cs40l26, CS40L26_VIBE_STATE_EVENT_ASP_START);
		ret = cs40l26_asp_start(cs40l26);
		mutex_unlock(&cs40l26->lock);
		if (ret)
			return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_ASP, __func__);

		if (!completion_done(&cs40l26->i2s_cont)) {
			if (!wait_for_completion_timeout(&cs40l26->i2s_cont,
					msecs_to_jiffies(CS40L26_ASP_START_TIMEOUT))) {
				dev_warn(codec->dev, "SVC calibration not complete\n");
				cs40l26_log_err(cs40l26, -ETIME, CS40L26_ERR_TYPE_DSP, __func__);
			}
		}

		ret = cs40l26_swap_ext_clk(codec, CS40L26_PLL_REFCLK_BCLK);
		if (ret)
			return ret;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		ret = cs40l26_swap_ext_clk(codec, CS40L26_PLL_REFCLK_MCLK);
		if (ret)
			return ret;

		mutex_lock(&cs40l26->lock);
		cs40l26_vibe_state_update(cs40l26, CS40L26_VIBE_STATE_EVENT_ASP_STOP);
		mutex_unlock(&cs40l26->lock);

		break;
	default:
		dev_err(dev, "Invalid event: %d\n", event);
		return -EINVAL;
	}

	return 0;
}

static int cs40l26_dsp_tx(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol, int event)
{	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_dapm_to_component(w->dapm));
	struct cs40l26_private *cs40l26 = codec->core;
	struct device *dev = cs40l26->dev;
	const struct firmware *fw;
	int ret;
	u32 reg;

	dev_dbg(dev, "%s: %s\n", __func__, event == SND_SOC_DAPM_POST_PMU ? "PMU" : "PMD");

	if (codec->dsp_bypass) {
		dev_err(dev, "Cannot use A2H while bypassing DSP\n");
		return cs40l26_log_err(cs40l26, -EPERM, CS40L26_ERR_TYPE_ASP, __func__);
	}

	ret = cl_dsp_get_reg(cs40l26->dsp, "A2HEN", CL_DSP_XM_UNPACKED_TYPE, CS40L26_A2H_ALGO_ID,
			&reg);
	if (ret)
		return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
#if defined(PREVENT_WAKEUP_BY_DAI_OPS)
		ret = regmap_update_bits(codec->regmap, CS40L26_ASP_FRAME_CONTROL5,
				CS40L26_ASP_RX1_SLOT_MASK | CS40L26_ASP_RX2_SLOT_MASK,
				codec->tdm_slot_a2h[0] | (codec->tdm_slot_a2h[1] << CS40L26_ASP_RX2_SLOT_SHIFT));
		if (ret) {
			dev_err(codec->dev, "Failed to update ASP A2H slot number\n");
		}
#endif
		if (codec->tuning != codec->tuning_prev) {
			ret = request_firmware(&fw, codec->bin_file, dev);
			if (ret) {
				dev_err(codec->dev, "Failed to request %s\n", codec->bin_file);
				return cs40l26_log_err(cs40l26, ret,
						CS40L26_ERR_TYPE_COEFF, __func__);
			}

			ret = cl_dsp_coeff_file_parse(cs40l26->dsp, fw);
			release_firmware(fw);
			if (ret) {
				dev_warn(dev, "Failed to load %s, %d. Continuing...",
						codec->bin_file, ret);
				return cs40l26_log_err(cs40l26, ret,
						CS40L26_ERR_TYPE_COEFF, __func__);
			}

			dev_info(dev, "%s Loaded Successfully\n", codec->bin_file);

			codec->tuning_prev = codec->tuning;

			ret = cs40l26_mailbox_write(cs40l26, CS40L26_DSP_MBOX_CMD_A2H_REINIT);
			if (ret)
				return ret;
		}
		ret = regmap_write(cs40l26->regmap, reg, 1);
		if (ret)
			return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);

		break;
	case SND_SOC_DAPM_PRE_PMD:
#if defined(PREVENT_WAKEUP_BY_DAI_OPS)
		ret = regmap_update_bits(codec->regmap, CS40L26_ASP_FRAME_CONTROL5,
				CS40L26_ASP_RX1_SLOT_MASK | CS40L26_ASP_RX2_SLOT_MASK,
				codec->tdm_slot[0] | (codec->tdm_slot[1] << CS40L26_ASP_RX2_SLOT_SHIFT));
		if (ret) {
			dev_err(codec->dev, "Failed to update ASP PCM slot number\n");
		}
#endif
		ret = regmap_write(cs40l26->regmap, reg, 0);
		if (ret)
			return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);

		break;
	default:
		dev_err(dev, "Invalid A2H event: %d\n", event);
		return cs40l26_log_err(cs40l26, -EINVAL, CS40L26_ERR_TYPE_ASP, __func__);
	}

	return 0;
}

static int cs40l26_sdout_ev(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol, int event)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_dapm_to_component(w->dapm));
	int ret;

	if (!device_property_present(codec->core->dev, "cirrus,asp-dout-enable")) {
		dev_err(codec->dev, "GP8 pin is not configured to function as SDOUT\n");
		return cs40l26_log_err(codec->core, -EPERM, CS40L26_ERR_TYPE_HW, __func__);
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!codec->asp_rx) {
			ret = regmap_set_bits(codec->regmap, CS40L26_ASP_ENABLES1,
					CS40L26_ASP_ENABLE_MASK);
			if (ret)
				return cs40l26_log_err(codec->core, ret,
						CS40L26_ERR_TYPE_CP, __func__);
		}

		ret = regmap_set_bits(codec->regmap, CS40L26_GPIO_PAD_CONTROL,
				CS40L26_GP8_SDOUT_MASK);
		if (ret)
			return cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_CP, __func__);

		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!codec->asp_rx) {
			ret = regmap_clear_bits(codec->regmap, CS40L26_ASP_ENABLES1,
					CS40L26_ASP_ENABLE_MASK);
			if (ret)
				return cs40l26_log_err(codec->core, ret,
						CS40L26_ERR_TYPE_CP, __func__);
		}

		ret = regmap_clear_bits(codec->regmap, CS40L26_GPIO_PAD_CONTROL,
				CS40L26_GP8_SDOUT_MASK);
		if (ret)
			return cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_CP, __func__);

		break;
	default:
		dev_err(codec->dev, "Invalid event: %d\n", event);
		return cs40l26_log_err(codec->core, -EINVAL, CS40L26_ERR_TYPE_ASP, __func__);
	}

	return 0;
}

static int cs40l26_asp_rx(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol, int event)
{	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_dapm_to_component(w->dapm));
	bool is_revid_b2 = (codec->core->revid == (CS40L26_REVID_B2)) ? true : false;
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	u32 flags = 0, reg = 0;
	u8 data_src;
	int ret;

	dev_dbg(dev, "%s: %s\n", __func__, event == SND_SOC_DAPM_POST_PMU ? "PMU" : "PMD");

	mutex_lock(&cs40l26->lock);

	data_src = codec->dsp_bypass ? CS40L26_DATA_SRC_ASPRX1 : CS40L26_DATA_SRC_DSP1TX1;

	if (is_revid_b2) {
		ret = cl_dsp_get_reg(cs40l26->dsp, "FLAGS", CL_DSP_XM_UNPACKED_TYPE,
				cs40l26->fw_id, &reg);
		if (ret) {
			cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);
			goto err_mutex;
		}

		ret = regmap_read(regmap, reg, &flags);
		if (ret) {
			cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
			goto err_mutex;
	}
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret = regmap_update_bits(regmap, CS40L26_DACPCM1_INPUT,
				CS40L26_DATA_SRC_MASK, data_src);
		if (ret) {
			dev_err(dev, "Failed to set DAC PCM input\n");
			cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
			goto err_mutex;
		}

		ret = regmap_set_bits(regmap, CS40L26_ASP_ENABLES1, CS40L26_ASP_ENABLE_MASK);
		if (ret) {
			cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
			goto err_mutex;
		}

		codec->asp_rx = true;

		/* Force open-loop if closed-loop not set */
		if (!(flags & CS40L26_FLAGS_I2S_SVC_EN_MASK) && is_revid_b2) {
			codec->svc_ol_forced = true;
			ret = regmap_set_bits(regmap, reg, CS40L26_FLAGS_I2S_SVC_EN_MASK |
					CS40L26_FLAGS_I2S_SVC_LOOP_MASK);
			if (ret) {
				cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
				goto err_mutex;
			}
		} else {
			codec->svc_ol_forced = false;
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		ret = cs40l26_mailbox_write(cs40l26, CS40L26_DSP_MBOX_CMD_STOP_I2S);
		if (ret)
			goto err_mutex;

		if (codec->svc_ol_forced) {
			ret = regmap_clear_bits(regmap, reg, CS40L26_FLAGS_I2S_SVC_EN_MASK |
					CS40L26_FLAGS_I2S_SVC_LOOP_MASK);
			if (ret) {
				cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
				goto err_mutex;
		}
		}

		ret = regmap_clear_bits(regmap, CS40L26_ASP_ENABLES1, CS40L26_ASP_ENABLE_MASK);
		if (ret) {
			cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
			goto err_mutex;
		}

		codec->asp_rx = false;

		break;
	default:
		dev_err(dev, "Invalid PCM event: %d\n", event);
		ret = -EINVAL;
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_ASP, __func__);
	}

err_mutex:
	mutex_unlock(&cs40l26->lock);

	return ret;
}

static int cs40l26_i2s_vmon_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	int ret;
	u32 val;

	ret = cs40l26_pm_enter(cs40l26->dev);
	if (ret)
		return ret;

	ret = regmap_read(cs40l26->regmap, CS40L26_SPKMON_VMON_DEC_OUT_DATA, &val);
	if (ret) {
		dev_err(cs40l26->dev, "Failed to get VMON Data for I2S\n");
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto pm_err;
	}

	if (val & CS40L26_VMON_OVFL_FLAG_MASK) {
		dev_err(cs40l26->dev, "I2S VMON overflow detected\n");
		ret = -EOVERFLOW;
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_HW, __func__);
		goto pm_err;
	}

	ucontrol->value.enumerated.item[0] = val & CS40L26_VMON_DEC_OUT_DATA_MASK;

pm_err:
	cs40l26_pm_exit(cs40l26->dev);

	return ret;
}

static int cs40l26_dsp_bypass_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;

	mutex_lock(&cs40l26->lock);

	if (codec->dsp_bypass)
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;

	mutex_unlock(&cs40l26->lock);

	return 0;
}

static int cs40l26_dsp_bypass_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;

	mutex_lock(&cs40l26->lock);

	if (ucontrol->value.enumerated.item[0])
		codec->dsp_bypass = true;
	else
		codec->dsp_bypass = false;

	mutex_unlock(&cs40l26->lock);

	return 0;
}

static int cs40l26_svc_en_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	unsigned int algo_id, val = 0, reg;
	struct device *dev = cs40l26->dev;
	int ret;

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	ret = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (ret)
		goto pm_err;

	ret = cl_dsp_get_reg(cs40l26->dsp, "FLAGS", CL_DSP_XM_UNPACKED_TYPE, algo_id, &reg);
	if (ret) {
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);
		goto pm_err;
	}

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		dev_err(cs40l26->dev, "Failed to read FLAGS\n");
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto pm_err;
	}

	if (val & CS40L26_SVC_EN_MASK)
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;

pm_err:
	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_svc_en_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int algo_id, reg;
	int ret;

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	ret = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (ret)
		goto pm_err;

	ret = cl_dsp_get_reg(cs40l26->dsp, "FLAGS", CL_DSP_XM_UNPACKED_TYPE, algo_id, &reg);
	if (ret) {
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);
		goto pm_err;
	}

	snd_soc_dapm_mutex_lock(dapm);

	ret = regmap_update_bits(regmap, reg, CS40L26_SVC_EN_MASK,
			ucontrol->value.enumerated.item[0]);
	if (ret) {
		dev_err(cs40l26->dev, "Failed to specify SVC for streaming\n");
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
	}

	snd_soc_dapm_mutex_unlock(dapm);

pm_err:
	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_dvl_en_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int val = 0, reg;
	int ret;

	ret = cl_dsp_get_reg(cs40l26->dsp, "EN", CL_DSP_XM_UNPACKED_TYPE, CS40L26_DVL_ALGO_ID,
			&reg);
	if (ret)
		return ret;

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		dev_err(cs40l26->dev, "Failed to read EN(DVL)\n");
		goto pm_err;
	}

	if (val & CS40L26_DVL_EN_MASK)
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;

	dev_dbg(cs40l26->dev, "%s: reg = 0x%08X, val = %d\n", __func__, reg, ucontrol->value.enumerated.item[0]);

pm_err:
	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_dvl_en_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int reg;
	int ret;

	ret = cl_dsp_get_reg(cs40l26->dsp, "EN", CL_DSP_XM_UNPACKED_TYPE, CS40L26_DVL_ALGO_ID,
			&reg);
	if (ret)
		return ret;

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	dev_dbg(cs40l26->dev, "%s: reg = 0x%08X, val = %d\n", __func__, reg, ucontrol->value.enumerated.item[0]);
	
	snd_soc_dapm_mutex_lock(dapm);

	ret = regmap_update_bits(regmap, reg, CS40L26_DVL_EN_MASK,
			ucontrol->value.enumerated.item[0]);
	if (ret)
		dev_err(cs40l26->dev, "Failed to specify DVL for streaming\n");

	snd_soc_dapm_mutex_unlock(dapm);

	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_invert_streaming_data_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	unsigned int algo_id, val = 0, reg;
	struct device *dev = cs40l26->dev;
	int ret;

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	ret = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (ret)
		goto pm_err;

	ret = cl_dsp_get_reg(cs40l26->dsp, "SOURCE_INVERT",
			CL_DSP_XM_UNPACKED_TYPE, algo_id, &reg);
	if (ret) {
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);
		goto pm_err;
	}

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		dev_err(cs40l26->dev, "Failed to read SOURCE_INVERT\n");
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto pm_err;
	}

	if (val)
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;

pm_err:
	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_invert_streaming_data_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int algo_id, reg;
	int ret;

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	ret = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (ret)
		goto pm_err;

	ret = cl_dsp_get_reg(cs40l26->dsp, "SOURCE_INVERT",
			CL_DSP_XM_UNPACKED_TYPE, algo_id, &reg);
	if (ret) {
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);
		goto pm_err;
	}

	snd_soc_dapm_mutex_lock(dapm);

	ret = regmap_write(regmap, reg, ucontrol->value.enumerated.item[0]);
	if (ret) {
		dev_err(cs40l26->dev, "Failed to specify invert streaming data\n");
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
	}

	snd_soc_dapm_mutex_unlock(dapm);

pm_err:
	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_tuning_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));

	ucontrol->value.enumerated.item[0] = codec->tuning;

	return 0;
}

static int cs40l26_tuning_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;

	if (ucontrol->value.enumerated.item[0] == codec->tuning)
		return 0;

	if (cs40l26->asp_enable)
		return cs40l26_log_err(cs40l26, -EBUSY, CS40L26_ERR_TYPE_DRIVER, __func__);

	codec->tuning = ucontrol->value.enumerated.item[0];

	memset(codec->bin_file, 0, PAGE_SIZE);
	codec->bin_file[PAGE_SIZE - 1] = '\0';

	if (codec->tuning > 0)
		snprintf(codec->bin_file, PAGE_SIZE, "cs40l26-a2h%d.bin", codec->tuning);
	else
		snprintf(codec->bin_file, PAGE_SIZE, "cs40l26-a2h.bin");

	return 0;
}

static int cs40l26_a2h_level_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int val = 0, reg;
	int ret;

	ret = cl_dsp_get_reg(cs40l26->dsp, "VOLUMELEVEL", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_A2H_ALGO_ID, &reg);
	if (ret)
		return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		dev_err(dev, "Failed to get VOLUMELEVEL\n");
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto pm_err;
	}

	ucontrol->value.integer.value[0] = val;

pm_err:
	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_a2h_level_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int val = 0, reg;
	int ret;

	ret = cl_dsp_get_reg(cs40l26->dsp, "VOLUMELEVEL", CL_DSP_XM_UNPACKED_TYPE,
			CS40L26_A2H_ALGO_ID, &reg);
	if (ret)
		return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	snd_soc_dapm_mutex_lock(dapm);

	if (ucontrol->value.integer.value[0] > CS40L26_A2H_LEVEL_MAX)
		val = CS40L26_A2H_LEVEL_MAX;
	else if (ucontrol->value.integer.value[0] < CS40L26_A2H_LEVEL_MIN)
		val = CS40L26_A2H_LEVEL_MIN;
	else
		val = ucontrol->value.integer.value[0];

	ret = regmap_write(regmap, reg, val);
	if (ret) {
		dev_err(dev, "Failed to set VOLUMELEVEL\n");
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
	}

	snd_soc_dapm_mutex_unlock(dapm);

	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_i2s_atten_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int val = 0, reg;
	int ret;

	ret = cl_dsp_get_reg(cs40l26->dsp, "I2S_ATTENUATION", CL_DSP_XM_UNPACKED_TYPE,
			cs40l26->fw_id, &reg);
	if (ret)
		return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto pm_err;
	}
#if defined(I2S_ATTENUATION_LINEARIZE)
	{
		int i;

		for (i = CS40L26_NUM_PCT_MAP_VALUES - 1; i >= 0; i--)
			if (cs40l26_attn_q21_2_vals[i] == val)
				break;

		if (i < 0)
			i = 0;

		val = i;
	}
#endif

	ucontrol->value.integer.value[0] = val;

pm_err:
	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_i2s_atten_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_soc_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(comp);
	struct cs40l26_codec *codec = snd_soc_component_get_drvdata(comp);
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	u32 val = 0, reg;
	int ret;

	ret = cl_dsp_get_reg(cs40l26->dsp, "I2S_ATTENUATION", CL_DSP_XM_UNPACKED_TYPE,
			cs40l26->fw_id, &reg);
	if (ret)
		return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	snd_soc_dapm_mutex_lock(dapm);

#if !defined(I2S_ATTENUATION_LINEARIZE)
	if (ucontrol->value.integer.value[0] > CS40L26_I2S_ATTENUATION_MAX)
		val = CS40L26_I2S_ATTENUATION_MAX;
	else if (ucontrol->value.integer.value[0] < 0)
		val = 0;
	else
		val = ucontrol->value.integer.value[0];
#else
	if (ucontrol->value.integer.value[0] > 100)
		val = 100;
	else if (ucontrol->value.integer.value[0] < 0)
		val = 0;
	else
		val = ucontrol->value.integer.value[0];

	val = cs40l26_attn_q21_2_vals[val];
#endif

	ret = regmap_write(regmap, reg, val);
	if (ret)
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);

	snd_soc_dapm_mutex_unlock(dapm);

	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_a2h_delay_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int val = 0, reg;
	int ret;

	ret = cl_dsp_get_reg(cs40l26->dsp, "LRADELAYSAMPS",
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_A2H_ALGO_ID, &reg);
	if (ret)
		return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		dev_err(dev, "Failed to get LRADELAYSAMPS\n");
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto err;
	}

	ucontrol->value.integer.value[0] = val;

err:
	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_a2h_delay_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int val = 0, reg;
	int ret;

	ret = cl_dsp_get_reg(cs40l26->dsp, "LRADELAYSAMPS",
			CL_DSP_XM_UNPACKED_TYPE, CS40L26_A2H_ALGO_ID, &reg);
	if (ret)
		return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	snd_soc_dapm_mutex_lock(dapm);

		val = ucontrol->value.integer.value[0];

	ret = regmap_write(regmap, reg, val);
	if (ret) {
		dev_err(dev, "Failed to set LRADELAYSAMPS\n");
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
	}

	snd_soc_dapm_mutex_unlock(dapm);

	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_boost_disable_delay_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	u32 algo_id, delay, reg;
	int ret;

	ret = cs40l26_pm_enter(cs40l26->dev);
	if (ret)
		return ret;

	ret = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (ret)
		goto pm_err;

	ret = cl_dsp_get_reg(cs40l26->dsp, "BOOST_DISABLE_DELAY", CL_DSP_XM_UNPACKED_TYPE,
			algo_id, &reg);
	if (ret) {
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_FW, __func__);
		goto pm_err;
	}

	ret = regmap_read(cs40l26->regmap, reg, &delay);
	if (ret) {
		cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto pm_err;
	}

	ucontrol->value.integer.value[0] = delay;

pm_err:
	cs40l26_pm_exit(cs40l26->dev);

	return ret;
}

static int cs40l26_boost_disable_delay_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_soc_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(comp);
	struct cs40l26_codec *codec = snd_soc_component_get_drvdata(comp);
	struct cs40l26_private *cs40l26 = codec->core;
	u32 algo_id, delay, reg;
	int ret;

	ret = cs40l26_pm_enter(cs40l26->dev);
	if (ret)
		return ret;

	ret = cs40l26_get_ram_ext_algo_id(cs40l26, &algo_id);
	if (ret)
		goto pm_err;

	ret = cl_dsp_get_reg(cs40l26->dsp, "BOOST_DISABLE_DELAY", CL_DSP_XM_UNPACKED_TYPE,
			algo_id, &reg);
	if (ret)
		goto pm_err;

	snd_soc_dapm_mutex_lock(dapm);

	delay = ucontrol->value.integer.value[0];

	ret = regmap_write(cs40l26->regmap, reg, delay);

	snd_soc_dapm_mutex_unlock(dapm);

pm_err:
	cs40l26_pm_exit(cs40l26->dev);

	return ret;
}

#if defined(REG_DBG)
static int cs40l26_reg_addr_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct device *dev = cs40l26->dev;


	ucontrol->value.integer.value[0] = cs40l26->reg_addr;
	dev_dbg(dev, "%s: get reg_addr = 0x%08lx\n", __func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int cs40l26_reg_addr_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct device *dev = cs40l26->dev;

	cs40l26->reg_addr = ucontrol->value.integer.value[0];
	dev_dbg(dev, "%s: set reg_addr = 0x%08x\n", __func__, cs40l26->reg_addr);

	return 0;
}

static int cs40l26_reg_val_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int val = 0;
	int ret;

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	ret = regmap_read(regmap, cs40l26->reg_addr, &val);
	if (ret) {
		dev_err(dev, "Failed to read register value\n");
		goto err;
	}

	ucontrol->value.integer.value[0] = val;

err:
	cs40l26_pm_exit(dev);

	return ret;
}

static int cs40l26_reg_val_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct cs40l26_private *cs40l26 = codec->core;
	struct regmap *regmap = cs40l26->regmap;
	struct device *dev = cs40l26->dev;
	unsigned int val = 0;
	int ret;

	ret = cs40l26_pm_enter(dev);
	if (ret)
		return ret;

	val = ucontrol->value.integer.value[0];
	
	ret = regmap_write(regmap, cs40l26->reg_addr, val);
	if (ret) {
		dev_err(dev, "Failed to write register value\n");
		goto err;
	}

err:
	cs40l26_pm_exit(dev);

	return ret;
}
#endif

#if defined(PREVENT_WAKEUP_BY_DAI_OPS)
static int cs40l26_asp_params(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol, int event)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_dapm_to_component(w->dapm));
	struct cs40l26_private *cs40l26 = codec->core;
	struct device *dev = cs40l26->dev;
	struct snd_soc_dai *dai = NULL;
	int ret;

	dev_dbg(codec->dev, "%s: %s\n", __func__, event == SND_SOC_DAPM_POST_PMU ? "PMU" : "PMD");
	
	for_each_component_dais(snd_soc_dapm_to_component(w->dapm), dai)
		if ((NULL != dai) && !strcmp("cs40l26-pcm", dai->name))
			break;
		else {
			dev_err(codec->dev, "%s: Can't find cs40l26-pcm dai\n", __func__);
			return -ENODEV;
		}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret = cs40l26_pm_enter(dev);
		if (ret)
			return ret;

		ret = regmap_update_bits(codec->regmap, CS40L26_MONITOR_FILT,
					   CS40L26_VIMON_DUAL_RATE_MASK,
					   FIELD_PREP(CS40L26_VIMON_DUAL_RATE_MASK, (96000 == codec->lrclk) ? 1 : 0));
		if (ret) {
		   dev_err(codec->dev, "Failed to update CS40L26_MONITOR_FILT\n");
		}
		
		ret = regmap_update_bits(codec->regmap, CS40L26_ASP_DATA_CONTROL5, CS40L26_ASP_RX_WL_MASK, codec->asp_rx_wl);
		if (ret) {
		   dev_err(codec->dev, "Failed to update ASP RX WL\n");
		}
		
		ret = regmap_update_bits(codec->regmap, CS40L26_ASP_CONTROL2,
				CS40L26_ASP_FSYNC_INV_MASK | CS40L26_ASP_BCLK_INV_MASK |
				CS40L26_ASP_FMT_MASK | CS40L26_ASP_RX_WIDTH_MASK, codec->daifmt | ((codec->asp_rx_width << CS40L26_ASP_RX_WIDTH_SHIFT) & CS40L26_ASP_RX_WIDTH_MASK));
		if (ret) {
			dev_err(codec->dev, "Failed to update ASP RX width\n");
		}

		ret = regmap_update_bits(codec->regmap, CS40L26_ASP_FRAME_CONTROL5,
				CS40L26_ASP_RX1_SLOT_MASK | CS40L26_ASP_RX2_SLOT_MASK,
				codec->tdm_slot[0] | (codec->tdm_slot[1] << CS40L26_ASP_RX2_SLOT_SHIFT));
		if (ret) {
			dev_err(codec->dev, "Failed to update ASP PCM slot number\n");
		}

		cs40l26_pm_exit(dev);
		break;
	default:
		dev_err(dev, "Invalid event: %d\n", event);
		return -EINVAL;
	}

	return 0;
}

static int cs40l26_asp_mode_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));

	ucontrol->value.enumerated.item[0] = (codec->daifmt & GENMASK(10, 8)) ? 0 : 1;

	return 0;
}

static int cs40l26_asp_mode_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct snd_soc_dai *dai = NULL;

	for_each_component_dais(snd_soc_kcontrol_component(kcontrol), dai)
		if ((NULL != dai) && !strcmp("cs40l26-pcm", dai->name))
			break;
		else {
			dev_err(codec->dev, "%s: Can't find cs40l26-pcm dai\n", __func__);
			return -ENODEV;
		}

	if (0 == ucontrol->value.enumerated.item[0])
		cs40l26_set_dai_fmt(dai, SND_SOC_DAIFMT_CBC_CFC|SND_SOC_DAIFMT_I2S);
	else if (1 == ucontrol->value.enumerated.item[0])
		cs40l26_set_dai_fmt(dai, SND_SOC_DAIFMT_CBC_CFC|SND_SOC_DAIFMT_DSP_A);
	else
		return -EINVAL;

	return 0;
}

static int cs40l26_asp_pcm_slot_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));

	ucontrol->value.enumerated.item[0] = codec->tdm_slot[0];

	return 0;
}

static int cs40l26_asp_pcm_slot_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct snd_soc_dai *dai = NULL;

	for_each_component_dais(snd_soc_kcontrol_component(kcontrol), dai)
		if ((NULL != dai) && !strcmp("cs40l26-pcm", dai->name))
			break;
		else {
			dev_err(codec->dev, "%s: Can't find cs40l26-pcm dai\n", __func__);
			return -ENODEV;
		}

	if (0 == ucontrol->value.enumerated.item[0])
		cs40l26_set_tdm_slot(dai, 0x3, 0x3, 2, 0);
	else if (1 == ucontrol->value.enumerated.item[0])
		cs40l26_set_tdm_slot(dai, 0xc, 0xc, 2, 0);
	else if (2 == ucontrol->value.enumerated.item[0])
		cs40l26_set_tdm_slot(dai, 0xc, 0xc, 2, 0);
	else if (3 == ucontrol->value.enumerated.item[0])
		cs40l26_set_tdm_slot(dai, 0x18, 0x18, 2, 0);
	else if (4 == ucontrol->value.enumerated.item[0])
		cs40l26_set_tdm_slot(dai, 0x30, 0x30, 2, 0);
	else if (5 == ucontrol->value.enumerated.item[0])
		cs40l26_set_tdm_slot(dai, 0x60, 0x60, 2, 0);
	else if (6 == ucontrol->value.enumerated.item[0])
		cs40l26_set_tdm_slot(dai, 0xc0, 0xc0, 2, 0);
	else if (7 == ucontrol->value.enumerated.item[0])
		cs40l26_set_tdm_slot(dai, 0x180, 0x180, 2, 0);
	else
		return -EINVAL;

	return 0;
}

static int cs40l26_asp_a2h_slot_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));

	ucontrol->value.enumerated.item[0] = codec->tdm_slot_a2h[0] / 2;

	return 0;
}

static int cs40l26_asp_a2h_slot_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(snd_soc_kcontrol_component(kcontrol));
	struct snd_soc_dai *dai = NULL;
	unsigned int rx_mask;

	for_each_component_dais(snd_soc_kcontrol_component(kcontrol), dai)
		if ((NULL != dai) && !strcmp("cs40l26-pcm", dai->name))
			break;
		else {
			dev_err(codec->dev, "%s: Can't find cs40l26-pcm dai\n", __func__);
			return -ENODEV;
		}

	if (0 == ucontrol->value.enumerated.item[0])
		rx_mask = 0x3;
	else if (1 == ucontrol->value.enumerated.item[0])
		rx_mask = 0xc;
	else if (2 == ucontrol->value.enumerated.item[0])
		rx_mask = 0x30;
	else if (3 == ucontrol->value.enumerated.item[0])
		rx_mask = 0xc0;
	else
		return -EINVAL;

	codec->tdm_slot_a2h[0] = ffs(rx_mask) - 1;
	rx_mask &= ~(1 << codec->tdm_slot_a2h[0]);
	codec->tdm_slot_a2h[1] = ffs(rx_mask) - 1;

	return 0;
}

static const char *const asp_mode[] = {"I2S", "DSP A"};
static const char * const asp_pcm_slot[] = { "0", "1", "2", "3", "4", "5", "6", "7"};
static const char * const asp_a2h_slot[] = { "0", "2", "4", "6"};

static const struct soc_enum asp_mode_enum = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(asp_mode), asp_mode);
static const struct soc_enum asp_pcm_slot_enum = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(asp_pcm_slot), asp_pcm_slot);
static const struct soc_enum asp_a2h_slot_enum = SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(asp_a2h_slot), asp_a2h_slot);
#endif

static const struct snd_kcontrol_new cs40l26_controls[] = {
#if defined(REG_DBG)
	SOC_SINGLE_EXT("REG ADDR", 0, 0, 0x7fffffff, 0, cs40l26_reg_addr_get, cs40l26_reg_addr_put),
	SOC_SINGLE_EXT("REG VAL", 0, 0, 0x7fffffff, 0, cs40l26_reg_val_get, cs40l26_reg_val_put),
#endif

#if defined(PREVENT_WAKEUP_BY_DAI_OPS)
	SOC_ENUM_EXT("ASP Mode", asp_mode_enum, cs40l26_asp_mode_get, cs40l26_asp_mode_put),
	SOC_ENUM_EXT("ASP PCM Slot", asp_pcm_slot_enum, cs40l26_asp_pcm_slot_get, cs40l26_asp_pcm_slot_put),
	SOC_ENUM_EXT("ASP A2H Slot", asp_a2h_slot_enum, cs40l26_asp_a2h_slot_get, cs40l26_asp_a2h_slot_put),
#endif
	SOC_SINGLE_EXT("A2H Tuning", 0, 0, CS40L26_A2H_MAX_TUNINGS, 0, cs40l26_tuning_get,
			cs40l26_tuning_put),
	SOC_SINGLE_EXT("A2H Level", 0, 0, CS40L26_A2H_LEVEL_MAX, 0, cs40l26_a2h_level_get,
			cs40l26_a2h_level_put),
	SOC_SINGLE_EXT("SVC Algo Enable", 0, 0, 1, 0, cs40l26_svc_en_get, cs40l26_svc_en_put),
	SOC_SINGLE_EXT("DVL Algo Enable", 0, 0, 1, 0, cs40l26_dvl_en_get, cs40l26_dvl_en_put),
	SOC_SINGLE_EXT("Invert streaming data", 0, 0, 1, 0, cs40l26_invert_streaming_data_get,
			cs40l26_invert_streaming_data_put),
	SOC_SINGLE_EXT("I2S VMON", 0, 0, CS40L26_VMON_DEC_OUT_DATA_MAX, 0,
			cs40l26_i2s_vmon_get, NULL),
	SOC_SINGLE_EXT("DSP Bypass", 0, 0, 1, 0, cs40l26_dsp_bypass_get, cs40l26_dsp_bypass_put),
	SOC_SINGLE_EXT("A2H Delay", 0, 0, CS40L26_A2H_DELAY_MS_MAX, 0, cs40l26_a2h_delay_get,
			cs40l26_a2h_delay_put),
	SOC_SINGLE_EXT("Boost Disable Delay", 0, 0, CS40L26_BOOST_DISABLE_DELAY_MAX, 0,
			cs40l26_boost_disable_delay_get, cs40l26_boost_disable_delay_put),
};

static const struct snd_kcontrol_new cs40l26_b2_controls[] = {
#if !defined(I2S_ATTENUATION_LINEARIZE)
	SOC_SINGLE_EXT("I2S Attenuation", 0, 0, CS40L26_I2S_ATTENUATION_MAX, 0,
			cs40l26_i2s_atten_get, cs40l26_i2s_atten_put),
#else
	SOC_SINGLE_EXT("I2S Attenuation", 0, 0, 100, 0,
			cs40l26_i2s_atten_get, cs40l26_i2s_atten_put),
#endif
};

static const char * const cs40l26_out_mux_texts[] = { "Off", "PCM", "A2H" };
static SOC_ENUM_SINGLE_VIRT_DECL(cs40l26_out_mux_enum, cs40l26_out_mux_texts);
static const struct snd_kcontrol_new cs40l26_out_mux =
		SOC_DAPM_ENUM("Haptics Source", cs40l26_out_mux_enum);

static const struct snd_soc_dapm_widget cs40l26_dapm_widgets[] = {
#if defined(PREVENT_WAKEUP_BY_DAI_OPS)
	SND_SOC_DAPM_SUPPLY_S("ASP Params", -1, SND_SOC_NOPM, 0, 0, cs40l26_asp_params,
			SND_SOC_DAPM_POST_PMU),
#endif
	SND_SOC_DAPM_SUPPLY_S("ASP PLL", 0, SND_SOC_NOPM, 0, 0, cs40l26_clk_en,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_AIF_IN("ASPRX1", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("ASPRX2", NULL, 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_PGA_E("PCM", SND_SOC_NOPM, 0, 0, NULL, 0, cs40l26_asp_rx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("A2H", SND_SOC_NOPM, 0, 0, NULL, 0, cs40l26_dsp_tx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MUX("Haptics Source", SND_SOC_NOPM, 0, 0, &cs40l26_out_mux),
	SND_SOC_DAPM_OUTPUT("OUT"),
	SND_SOC_DAPM_INPUT("VSNS"),

	SND_SOC_DAPM_AIF_OUT_E("SDOUT", NULL, 0, SND_SOC_NOPM, 0, 0,
		cs40l26_sdout_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route cs40l26_dapm_routes[] = {
#if defined(PREVENT_WAKEUP_BY_DAI_OPS)
	{ "ASP Playback", NULL, "ASP Params" },
#endif
	{ "ASP Playback", NULL, "ASP PLL" },
	{ "ASPRX1", NULL, "ASP Playback" },
	{ "ASPRX2", NULL, "ASP Playback" },

	{ "PCM", NULL, "ASPRX1" },
	{ "PCM", NULL, "ASPRX2" },
	{ "A2H", NULL, "PCM" },

	{ "Haptics Source", "PCM", "PCM" },
	{ "Haptics Source", "A2H", "A2H" },
	{ "OUT", NULL, "Haptics Source" },

	{ "SDOUT", NULL, "VSNS" },
	{ "ASP Capture", NULL, "SDOUT" },
};

static int cs40l26_component_set_sysclk(struct snd_soc_component *component,
		int clk_id, int source, unsigned int freq, int dir)
{
	struct cs40l26_codec *codec = snd_soc_component_get_drvdata(component);
	struct device *dev = codec->dev;
	u8 clk_cfg;
	int ret;

	dev_dbg(dev, "%s: clk_id = %d, freq = %u\n", __func__, clk_id, freq);

	ret = cs40l26_get_clk_config((u32) (CS40L26_PLL_CLK_FREQ_MASK & freq), &clk_cfg);
	if (ret) {
		dev_err(dev, "Invalid Clock Frequency: %u Hz\n", freq);
		return cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_ASP, __func__);
	}

	if (clk_id != 0) {
		dev_err(dev, "Invalid Input Clock (ID: %d)\n", clk_id);
		return cs40l26_log_err(codec->core, -EINVAL, CS40L26_ERR_TYPE_ASP, __func__);
	}

	codec->sysclk_rate = (u32) (CS40L26_PLL_CLK_FREQ_MASK & freq);

	return 0;
}

static int cs40l26_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(codec_dai->component);

	dev_dbg(codec->dev, "%s: fmt = 0x%x\n", __func__, fmt);
	
	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
		dev_err(codec->dev, "Device can not be master\n");
		return cs40l26_log_err(codec->core, -EINVAL, CS40L26_ERR_TYPE_ASP, __func__);
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		codec->daifmt = 0;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		codec->daifmt = CS40L26_ASP_FSYNC_INV_MASK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		codec->daifmt = CS40L26_ASP_BCLK_INV_MASK;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		codec->daifmt = CS40L26_ASP_FSYNC_INV_MASK | CS40L26_ASP_BCLK_INV_MASK;
		break;
	default:
		dev_err(codec->dev, "Invalid DAI clock INV\n");
		return cs40l26_log_err(codec->core, -EINVAL, CS40L26_ERR_TYPE_ASP, __func__);
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		codec->daifmt |= ((CS40L26_ASP_FMT_TDM1_DSPA << CS40L26_ASP_FMT_SHIFT) &
				CS40L26_ASP_FMT_MASK);
		break;
	case SND_SOC_DAIFMT_I2S:
		codec->daifmt |= ((CS40L26_ASP_FMT_I2S << CS40L26_ASP_FMT_SHIFT) &
				CS40L26_ASP_FMT_MASK);
		break;
	default:
		dev_err(codec->dev, "Invalid DAI format: 0x%X\n", fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return cs40l26_log_err(codec->core, -EINVAL, CS40L26_ERR_TYPE_ASP, __func__);
	}

	return 0;
}

#if defined(PREVENT_WAKEUP_BY_DAI_OPS)
static int cs40l26_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct cs40l26_codec *codec = snd_soc_component_get_drvdata(dai->component);
	int ret;

	dev_dbg(codec->dev, "%s: asp_wl=%d, asp_width=%d, rate=%d, channels=%d\n", __func__,
				params_width(params), params_physical_width(params), params_rate(params), params_channels(params));

#if defined(CUSTOMIZED_I2S)
	ret = cs40l26_component_set_sysclk(dai->component, 0, 0, 4 * params_rate(params) * (codec->tdm_width ? codec->tdm_width : params_physical_width(params)), 0);
#else
	ret = cs40l26_component_set_sysclk(dai->component, 0, 0, params_channels(params) * params_rate(params) * (codec->tdm_width ? codec->tdm_width : params_physical_width(params)), 0);
#endif

	codec->lrclk = params_rate(params);;

	codec->asp_rx_wl = (u8)(params_width(params) & 0xFF);
	
	if (!codec->tdm_width)
		//codec->asp_rx_width = asp_rx_wl;
		codec->asp_rx_width = (u8) (params_physical_width(params) & 0xFF);
	else
		codec->asp_rx_width = (u8) (codec->tdm_width & 0xFF);

	return ret;
}
#else
static int cs40l26_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct cs40l26_codec *codec = snd_soc_component_get_drvdata(dai->component);
	u32 asp_rx_wl, asp_rx_width;
	int ret;

	dev_dbg(codec->dev, "%s: asp_wl=%d, asp_width=%d, rate=%d, channels=%d\n", __func__,
				params_width(params), params_physical_width(params), params_rate(params), params_channels(params));

#if defined(CUSTOMIZED_I2S)
	cs40l26_set_tdm_slot(dai, 0xc, 0xc, 2, 0); // Haptic on slot2
	//cs40l26_set_tdm_slot(dai, 0x3, 0x3, 2, 0); // Haptic on slot0
	//cs40l26_set_dai_fmt(dai, SND_SOC_DAIFMT_CBC_CFC|SND_SOC_DAIFMT_I2S);
	cs40l26_set_dai_fmt(dai, SND_SOC_DAIFMT_CBC_CFC|SND_SOC_DAIFMT_DSP_A);
	cs40l26_component_set_sysclk(dai->component, 0, 0, 4 * params_rate(params) * (codec->tdm_width ? codec->tdm_width : params_physical_width(params)), 0);
#else
	cs40l26_set_tdm_slot(dai, 3, 3, 2, 0);
	//cs40l26_set_dai_fmt(dai, SND_SOC_DAIFMT_CBC_CFC|SND_SOC_DAIFMT_I2S);
	cs40l26_set_dai_fmt(dai, SND_SOC_DAIFMT_CBC_CFC|SND_SOC_DAIFMT_DSP_A);
	cs40l26_component_set_sysclk(dai->component, 0, 0, params_channels(params) * params_rate(params) * (codec->tdm_width ? codec->tdm_width : params_physical_width(params)), 0);
#endif

	ret = cs40l26_pm_enter(codec->dev);
	if (ret)
		return ret;

	switch (params_rate(params)) {
	case 48000:
		ret = regmap_clear_bits(codec->regmap, CS40L26_MONITOR_FILT,
				CS40L26_VIMON_DUAL_RATE_MASK);
		break;
	case 96000:
		ret = regmap_set_bits(codec->regmap, CS40L26_MONITOR_FILT,
				CS40L26_VIMON_DUAL_RATE_MASK);
		break;
	default:
		dev_err(codec->dev, "Invalid sample rate: %d Hz\n", params_rate(params));
		ret = -EINVAL;
		cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_ASP, __func__);
		goto err_pm;
	}
	if (ret) {
		dev_err(codec->dev, "%s Failed\n", __func__);
		cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto err_pm;
	}

	asp_rx_wl = (u8) (params_width(params) & 0xFF);
	ret = regmap_update_bits(codec->regmap, CS40L26_ASP_DATA_CONTROL5,
			CS40L26_ASP_RX_WL_MASK, asp_rx_wl);
	if (ret) {
		dev_err(codec->dev, "Failed to update ASP RX WL\n");
		cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto err_pm;
	}

	if (!codec->tdm_width)
		//asp_rx_width = asp_rx_wl;
		asp_rx_width = (u8) (params_physical_width(params) & 0xFF);
	else
		asp_rx_width = (u8) (codec->tdm_width & 0xFF);

	codec->daifmt |= ((asp_rx_width << CS40L26_ASP_RX_WIDTH_SHIFT) &
			CS40L26_ASP_RX_WIDTH_MASK);

	ret = regmap_update_bits(codec->regmap, CS40L26_ASP_CONTROL2,
			CS40L26_ASP_FSYNC_INV_MASK | CS40L26_ASP_BCLK_INV_MASK |
			CS40L26_ASP_FMT_MASK | CS40L26_ASP_RX_WIDTH_MASK, codec->daifmt);
	if (ret) {
		dev_err(codec->dev, "Failed to update ASP RX width\n");
		cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto err_pm;
	}

	ret = regmap_update_bits(codec->regmap, CS40L26_ASP_FRAME_CONTROL5,
			CS40L26_ASP_RX1_SLOT_MASK | CS40L26_ASP_RX2_SLOT_MASK,
			codec->tdm_slot[0] | (codec->tdm_slot[1] << CS40L26_ASP_RX2_SLOT_SHIFT));
	if (ret) {
		dev_err(codec->dev, "Failed to update ASP slot number\n");
		cs40l26_log_err(codec->core, ret, CS40L26_ERR_TYPE_CP, __func__);
		goto err_pm;
	}

	dev_dbg(codec->dev, "ASP: %d bits in %d bit slots, slot #s: %d, %d\n",
			asp_rx_wl, asp_rx_width, codec->tdm_slot[0], codec->tdm_slot[1]);

err_pm:
	cs40l26_pm_exit(codec->dev);

	return ret;
}
#endif

static int cs40l26_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
		unsigned int rx_mask, int slots, int slot_width)
{
	struct cs40l26_codec *codec =
			snd_soc_component_get_drvdata(dai->component);

	dev_dbg(codec->dev, "%s: tx_mask = 0x%x, rx_mask = 0x%x, slots = %d, slot_width = %d\n", __func__, tx_mask, rx_mask, slots, slot_width);
	
	codec->tdm_width = slot_width;
	codec->tdm_slots = slots;

	/* Reset to slots 0,1 if TDM is being disabled, and catch the case
	 * where both RX1 and RX2 would be set to slot 0 since that causes
	 * hardware to flag an error
	 */
	if (!slots || rx_mask == 0x1)
		rx_mask = 0x3;

	codec->tdm_slot[0] = ffs(rx_mask) - 1;
	rx_mask &= ~(1 << codec->tdm_slot[0]);
	codec->tdm_slot[1] = ffs(rx_mask) - 1;

	return 0;
}

static const struct snd_soc_dai_ops cs40l26_dai_ops = {
	.set_fmt = cs40l26_set_dai_fmt,
	.set_tdm_slot = cs40l26_set_tdm_slot,
	.hw_params = cs40l26_pcm_hw_params,
};

static struct snd_soc_dai_driver cs40l26_dai[] = {
	{
		.name = "cs40l26-pcm",
		.id = 0,
		.playback = {
			.stream_name = "ASP Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS40L26_RATES,
			.formats = CS40L26_FORMATS,
		},
		.capture = {
			.stream_name = "ASP Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS40L26_RATES,
			.formats = CS40L26_FORMATS,
		},
		.ops = &cs40l26_dai_ops,
		.symmetric_rate = 1,
	},
};

static int cs40l26_codec_probe(struct snd_soc_component *component)
{
	struct cs40l26_codec *codec = snd_soc_component_get_drvdata(component);

	codec->bin_file = devm_kzalloc(codec->dev, PAGE_SIZE, GFP_KERNEL);
	if (!codec->bin_file)
		return -ENOMEM;

	codec->bin_file[PAGE_SIZE - 1] = '\0';
	snprintf(codec->bin_file, PAGE_SIZE, "cs40l26-a2h.bin");

	/* Default audio SCLK frequency */
	codec->sysclk_rate = CS40L26_PLL_CLK_FRQ_1536000;

	codec->tdm_slot[0] = 0;
	codec->tdm_slot[1] = 1;

	if (codec->core->revid == CS40L26_REVID_B2)
		snd_soc_add_component_controls(component, cs40l26_b2_controls,
				ARRAY_SIZE(cs40l26_b2_controls));

	return 0;
}

static const struct snd_soc_component_driver soc_codec_dev_cs40l26 = {
	.probe = cs40l26_codec_probe,
	.set_sysclk = cs40l26_component_set_sysclk,

	.dapm_widgets = cs40l26_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cs40l26_dapm_widgets),
	.dapm_routes = cs40l26_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(cs40l26_dapm_routes),
	.controls = cs40l26_controls,
	.num_controls = ARRAY_SIZE(cs40l26_controls),
};

static int cs40l26_codec_driver_probe(struct platform_device *pdev)
{
	struct cs40l26_private *cs40l26 = dev_get_drvdata(pdev->dev.parent);
	struct cs40l26_codec *codec;
	int ret;

	codec = devm_kzalloc(&pdev->dev, sizeof(struct cs40l26_codec), GFP_KERNEL);
	if (!codec)
		return -ENOMEM;

	codec->core = cs40l26;
	codec->regmap = cs40l26->regmap;
	codec->dev = &pdev->dev;

	platform_set_drvdata(pdev, codec);

#if !defined(PREVENT_WAKEUP_BY_DAI_OPS)
	pm_runtime_enable(&pdev->dev);
#endif

#if defined(PREVENT_WAKEUP_BY_DAI_OPS)
	codec->tdm_slot[0] = 0;
	codec->tdm_slot[1] = 1;
	codec->tdm_slot_a2h[0] = 0;
	codec->tdm_slot_a2h[1] = 1;
#endif

	ret = snd_soc_register_component(&pdev->dev, &soc_codec_dev_cs40l26,
			cs40l26_dai, ARRAY_SIZE(cs40l26_dai));
	if (ret) {
		dev_err(&pdev->dev, "Failed to register codec: %d\n", ret);
		return cs40l26_log_err(cs40l26, ret, CS40L26_ERR_TYPE_DRIVER, __func__);
	}

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,0)
static void cs40l26_codec_driver_remove(struct platform_device *pdev)
{
	struct cs40l26_codec *codec = dev_get_drvdata(&pdev->dev);

#if !defined(PREVENT_WAKEUP_BY_DAI_OPS)
	pm_runtime_disable(codec->dev);
#endif

	snd_soc_unregister_component(codec->dev);
}
#else
static int cs40l26_codec_driver_remove(struct platform_device *pdev)
{
	struct cs40l26_codec *codec = dev_get_drvdata(&pdev->dev);

#if !defined(PREVENT_WAKEUP_BY_DAI_OPS)
	pm_runtime_disable(codec->dev);
#endif

	snd_soc_unregister_component(codec->dev);

	return 0;
}
#endif

static struct platform_driver cs40l26_codec_driver = {
	.driver = {
		.name = "cs40l26-codec",
	},
	.probe = cs40l26_codec_driver_probe,
	.remove = cs40l26_codec_driver_remove,
};
module_platform_driver(cs40l26_codec_driver);

MODULE_DESCRIPTION("ASoC CS40L26 driver");
MODULE_AUTHOR("Fred Treven <fred.treven@cirrus.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cs40l26-codec");
