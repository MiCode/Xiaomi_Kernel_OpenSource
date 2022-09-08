// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * cs35l45.c - CS35L45 ALSA SoC audio driver
 *
 * Copyright 2019 Cirrus Logic, Inc.
 *
 * Author: James Schulman <james.schulman@cirrus.com>
 *
 */

#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "wm_adsp.h"
#include "cs35l45.h"
#include <sound/cs35l45.h>

#define DRV_NAME "cs35l45"
#include <mtk-sp-spk-amp.h>

static struct wm_adsp_ops cs35l45_halo_ops;
static int (*cs35l45_halo_start_core)(struct wm_adsp *dsp);

static int __cs35l45_initialize(struct cs35l45_private *cs35l45);
static int cs35l45_hibernate(struct cs35l45_private *cs35l45, bool hiber_en);
static int cs35l45_set_sysclk(struct cs35l45_private *cs35l45, int clk_id,
			      unsigned int freq);
static int cs35l45_gpio_configuration(struct cs35l45_private *cs35l45);
static int cs35l45_activate_ctl(struct cs35l45_private *cs35l45,
				const char *ctl_name, bool active);
static int cs35l45_buffer_update_avail(struct cs35l45_private *cs35l45);
static void cs35l45_pm_runtime_setup(struct cs35l45_private *cs35l45);

struct cs35l45_mixer_cache {
	unsigned int reg;
	unsigned int mask;
	unsigned int val;
};

static int mt_cirrus_tdm_snd_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params,
				       struct snd_soc_dai *dai)
{
	int ret = 0;
	unsigned int slot_width = 32; //TDM_SLOT_WIDTH_BITS = 32
	unsigned int slots;
	unsigned int slot_mask, rate, clk_freq;
	/* Calculate slots and clk */
	rate = params_rate(params);
	//slots = 4; //TDM_MAX_SLOTS = 4
	slots = params_channels(params);
	slot_mask = 0x0000FFFF >> (16 - slots);
	clk_freq = rate * slot_width * slots;
	pr_info("%s: rate=%d, clk_freq=%d\n", __func__, rate, clk_freq);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_info("%s: name=: %s\n", __func__, dai->name);
		ret = snd_soc_dai_set_tdm_slot(dai,
					       slot_mask, slot_mask,
					       slots, slot_width);
		if (ret < 0) {
			pr_info("%s: failed to set tdm rx slot, err:%d\n",
				__func__, ret);
			goto end;
		}
		ret = snd_soc_dai_set_fmt(dai,
					  SND_SOC_DAIFMT_DSP_A |
					  SND_SOC_DAIFMT_CBS_CFS |
					  SND_SOC_DAIFMT_NB_NF);
		if (ret != 0) {
			pr_info("%s: Failed to set %s's fmt: ret = %d\n",
				dai->name, ret);
			return ret;
		}
		pr_info("%s: clk_freq: %d\n", __func__, clk_freq);

		ret = snd_soc_component_set_sysclk(
						   dai->component, 0, 0,
						   clk_freq,
						   SND_SOC_CLOCK_IN);
		if (ret < 0)
			pr_info("%s: set sysclk failed, err:%d\n",
				__func__, ret);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* TODO: Nothing to do here? */
		pr_info("%s: SNDRV_PCM_STREAM_CAPTURE, Do nothing.\n", __func__);
	} else {
		ret = -EINVAL;
		pr_info("%s: invalid use case, err:%d\n",
			__func__, ret);
		goto end;
	}
end:
	return ret;
}

static int cs35l45_supported_devid(struct cs35l45_private *cs35l45)
{
	unsigned int dev_id, rev_id, rel_id, otp_id;
	int ret = 0;

	ret = regmap_read(cs35l45->regmap, CS35L45_DEVID, &dev_id);
	if (ret < 0) {
		dev_info(cs35l45->dev, "Get Device ID failed\n");
		return ret;
	}

	ret = regmap_read(cs35l45->regmap, CS35L45_REVID, &rev_id);
	if (ret < 0) {
		dev_info(cs35l45->dev, "Get Revision ID failed\n");
		return ret;
	}

	ret = regmap_read(cs35l45->regmap, CS35L45_RELID, &rel_id);
	if (ret < 0) {
		dev_info(cs35l45->dev, "Get Software ID failed\n");
		return ret;
	}

	ret = regmap_read(cs35l45->regmap, CS35L45_OTPID, &otp_id);
	if (ret < 0) {
		dev_info(cs35l45->dev, "Get OTP ID failed\n");
		return ret;
	}

	switch (dev_id) {
	case CS35L45_SUPPORTED_ID_35A450:
		dev_info(cs35l45->dev,
			 "Cirrus Logic CS35L45: DEVID %02X REVID 0x%02X RELID 0x%02X OTPID 0x%02X.\n",
			 dev_id, rev_id, rel_id, otp_id);
		break;
	case CS35L45_SUPPORTED_ID_35A460:
		dev_info(cs35l45->dev,
			 "Cirrus Logic CS35L46: DEVID %02X REVID 0x%02X RELID 0x%02X OTPID 0x%02X.\n",
			 dev_id, rev_id, rel_id, otp_id);
		break;
	default:
		dev_info(cs35l45->dev,
			"DEVID %02X not supported. REVID 0x%02X RELID 0x%02X OTPID 0x%02X.\n",
			dev_id, rev_id, rel_id, otp_id);
		return -EINVAL;
	}

	return 0;
}

static bool cs35l45_is_csplmboxsts_correct(enum cspl_mboxcmd cmd,
					   enum cspl_mboxstate sts)
{
	switch (cmd) {
	case CSPL_MBOX_CMD_NONE:
	case CSPL_MBOX_CMD_UNKNOWN_CMD:
		return true;
	case CSPL_MBOX_CMD_PAUSE:
		return (sts == CSPL_MBOX_STS_PAUSED);
	case CSPL_MBOX_CMD_RESUME:
		return (sts == CSPL_MBOX_STS_RUNNING);
	case CSPL_MBOX_CMD_REINIT:
		return (sts == CSPL_MBOX_STS_RUNNING);
	case CSPL_MBOX_CMD_STOP_PRE_REINIT:
		return (sts == CSPL_MBOX_STS_RDY_FOR_REINIT);
	case CSPL_MBOX_CMD_HIBERNATE:
		return (sts == CSPL_MBOX_STS_HIBERNATE);
	case CSPL_MBOX_CMD_OUT_OF_HIBERNATE:
		return (sts == CSPL_MBOX_STS_PAUSED);
	default:
		return false;
	}
}

int cs35l45_set_csplmboxcmd(struct cs35l45_private *cs35l45,
			    enum cspl_mboxcmd cmd)
{
	unsigned int sts;
	int ret;

	reinit_completion(&cs35l45->virt2_mbox_comp);

	regmap_write(cs35l45->regmap, CS35L45_DSP_VIRT1_MBOX_1, cmd);

	ret = wait_for_completion_timeout(&cs35l45->virt2_mbox_comp,
					  msecs_to_jiffies(100));
	if (ret == 0) {
		dev_info(cs35l45->dev, "Timeout waiting for MBOX ACK\n");
		return -ETIMEDOUT;
	}

	regmap_read(cs35l45->regmap, CS35L45_DSP_MBOX_2, &sts);
	if (!cs35l45_is_csplmboxsts_correct(cmd, (enum cspl_mboxstate)sts)) {
		dev_info(cs35l45->dev, "Failed to set MBOX (cmd: %u, sts: %u)\n",
			cmd, sts);
		return -ENOMSG;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cs35l45_set_csplmboxcmd);

static void cs35l45_dsp_pmu_work(struct work_struct *work)
{
	struct cs35l45_private *cs35l45 = container_of(work,
						       struct cs35l45_private,
						       dsp_pmu_work);

	mutex_lock(&cs35l45->dsp_power_lock);
	cs35l45_set_csplmboxcmd(cs35l45, CSPL_MBOX_CMD_RESUME);
	mutex_unlock(&cs35l45->dsp_power_lock);
}

static void cs35l45_dsp_pmd_work(struct work_struct *work)
{
	struct cs35l45_private *cs35l45 = container_of(work,
						       struct cs35l45_private,
						       dsp_pmd_work);
	__be32 state;
	int i;

	mutex_lock(&cs35l45->dsp_power_lock);
	cs35l45_set_csplmboxcmd(cs35l45, CSPL_MBOX_CMD_PAUSE);

	usleep_range(10000, 11000);

	for (i = 0; i < 10; i++) {
		wm_adsp_read_ctl(&cs35l45->dsp, "STATE", WMFW_ADSP2_XM,
				 CS35L45_ALGID_PAUSE_RESUME, &state,
				 sizeof(__be32));

		if (be32_to_cpu(state) == CSPL_PR_PAUSED)
			break;

		usleep_range(10000, 11000);
	}

	if (i == 10) {
		dev_info(cs35l45->dev, "PAUSE_RESUME STATE (%d) is not paused\n",
			be32_to_cpu(state));
	}

	mutex_unlock(&cs35l45->dsp_power_lock);
}

static int cs35l45_dsp_loader_ev(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (cs35l45->dsp.booted) {
			dev_info(cs35l45->dev, "DSP already booted\n");
			return -EPERM;
		}

		regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
				   CS35L45_PLL_FORCE_EN_MASK,
				   CS35L45_PLL_FORCE_EN_MASK);

		wm_adsp_early_event(w, kcontrol, event);
		break;
	case SND_SOC_DAPM_POST_PMU:
		wm_adsp_event(w, kcontrol, event);

		regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
				   CS35L45_PLL_FORCE_EN_MASK, 0);
		break;
	default:
		dev_info(cs35l45->dev, "Invalid event = 0x%x\n", event);
		return -EINVAL;
	}

	return 0;
}

static int cs35l45_dsp_boot_ev(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);
	int ret;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (!cs35l45->dsp.booted) {
			dev_info(cs35l45->dev, "Preload DSP before boot\n");
			return -EPERM;
		}

		regmap_update_bits(cs35l45->regmap, CS35L45_PWRMGT_CTL,
				   CS35L45_MEM_RDY_MASK,
				   CS35L45_MEM_RDY_MASK);

		regmap_write(cs35l45->regmap, CS35L45_DSP1_CCM_CORE_CONTROL,
			     CS35L45_CCM_PM_REMAP_MASK |
			     CS35L45_CCM_CORE_RESET_MASK);

		reinit_completion(&cs35l45->virt2_mbox_comp);

		(*cs35l45_halo_start_core)(&cs35l45->dsp);

		ret = wait_for_completion_timeout(&cs35l45->virt2_mbox_comp,
						  msecs_to_jiffies(100));
		if (ret == 0) {
			dev_info(cs35l45->dev, "Timeout waiting for MBOX ACK\n");
			return -ETIMEDOUT;
		}

		ret = cs35l45_gpio_configuration(cs35l45);
		if (ret < 0) {
			dev_info(cs35l45->dev,
				"Failed to apply GPIO config (%d)\n", ret);
			return ret;
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		flush_work(&cs35l45->dsp_pmd_work);

		regmap_update_bits(cs35l45->regmap,
				   CS35L45_DSP1_STREAM_ARB_TX1_CONFIG_0,
				   CS35L45_DSP1_STREAM_ARB_TX1_EN_MASK, 0);

		regmap_update_bits(cs35l45->regmap,
				   CS35L45_DSP1_STREAM_ARB_MSTR1_CONFIG_0,
				   CS35L45_DSP1_STREAM_ARB_MSTR0_EN_MASK, 0);

		wm_adsp_early_event(w, kcontrol, event);
		wm_adsp_event(w, kcontrol, event);

		regmap_update_bits(cs35l45->regmap, CS35L45_PWRMGT_CTL,
				   CS35L45_MEM_RDY_MASK, 0);
		break;
	default:
		dev_info(cs35l45->dev, "Invalid event = 0x%x\n", event);
		return -EINVAL;
	}

	return 0;
}

static int cs35l45_dsp_power_ev(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (!cs35l45->dsp.running) {
			dev_info(cs35l45->dev, "DSP not running\n");
			return -EPERM;
		}

		flush_work(&cs35l45->dsp_pmd_work);
		queue_work(system_unbound_wq, &cs35l45->dsp_pmu_work);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (!cs35l45->dsp.running) {
			dev_info(cs35l45->dev, "DSP not running\n");
			return -EPERM;
		}

		flush_work(&cs35l45->dsp_pmu_work);
		queue_work(system_unbound_wq, &cs35l45->dsp_pmd_work);
		break;
	default:
		dev_info(cs35l45->dev, "Invalid event = 0x%x\n", event);
		ret = -EINVAL;
	}

	return ret;
}

static int cs35l45_hibernate_dapm(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (pm_runtime_suspended(cs35l45->dev)) {
			dev_dbg(cs35l45->dev, "Resume suspended AMP.\n");
			pm_runtime_resume(cs35l45->dev);
		}
		break;

	default:
		dev_info(cs35l45->dev, "Invalid event = 0x%x\n", event);
		ret = -EINVAL;
	}
	return ret;
}

static int cs35l45_global_en_ev(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);
	unsigned int val;
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(cs35l45->regmap, CS35L45_GLOBAL_ENABLES,
			     CS35L45_GLOBAL_EN_MASK, CS35L45_GLOBAL_EN_MASK);

		usleep_range(5000, 5100);

		regmap_read(cs35l45->regmap, CS35L45_BLOCK_ENABLES, &val);

		val = (val & CS35L45_BST_EN_MASK) >> CS35L45_BST_EN_SHIFT;
		if (val == CS35L45_BST_DISABLE_FET_OFF)
			regmap_update_bits(cs35l45->regmap,
					   CS35L45_BLOCK_ENABLES,
					   CS35L45_BST_EN_MASK,
					   CS35L45_BST_DISABLE_FET_ON <<
					   CS35L45_BST_EN_SHIFT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		regmap_read(cs35l45->regmap, CS35L45_BLOCK_ENABLES, &val);

		val = (val & CS35L45_BST_EN_MASK) >> CS35L45_BST_EN_SHIFT;
		if (val == CS35L45_BST_DISABLE_FET_ON)
			regmap_update_bits(cs35l45->regmap,
					   CS35L45_BLOCK_ENABLES,
					   CS35L45_BST_EN_MASK,
					   CS35L45_BST_DISABLE_FET_OFF <<
					   CS35L45_BST_EN_SHIFT);

		usleep_range(3000, 3100);

		regmap_update_bits(cs35l45->regmap, CS35L45_GLOBAL_ENABLES,
			     CS35L45_GLOBAL_EN_MASK, 0);
		break;
	default:
		dev_info(cs35l45->dev, "Invalid event = 0x%x\n", event);
		ret = -EINVAL;
	}

	return ret;
}

static const char * const pcm_tx_txt[] = {"Zero", "ASP_RX1", "ASP_RX2", "VMON",
			"IMON", "ERR_VOL", "VDD_BATTMON", "VDD_BSTMON",
			"DSP_TX1", "DSP_TX2"};

static const unsigned int pcm_tx_val[] = {CS35L45_PCM_SRC_ZERO,
			CS35L45_PCM_SRC_ASP_RX1, CS35L45_PCM_SRC_ASP_RX2,
			CS35L45_PCM_SRC_VMON, CS35L45_PCM_SRC_IMON,
			CS35L45_PCM_SRC_ERR_VOL, CS35L45_PCM_SRC_VDD_BATTMON,
			CS35L45_PCM_SRC_VDD_BSTMON, CS35L45_PCM_SRC_DSP_TX1,
			CS35L45_PCM_SRC_DSP_TX2};

static const char * const pcm_rx_txt[] = {"Zero", "ASP_RX1", "ASP_RX2", "VMON",
			"IMON", "ERR_VOL", "CLASSH_TGT", "VDD_BATTMON",
			"VDD_BSTMON", "TEMPMON"};

static const unsigned int pcm_rx_val[] = {CS35L45_PCM_SRC_ZERO,
			CS35L45_PCM_SRC_ASP_RX1, CS35L45_PCM_SRC_ASP_RX2,
			CS35L45_PCM_SRC_VMON, CS35L45_PCM_SRC_IMON,
			CS35L45_PCM_SRC_ERR_VOL, CS35L45_PCM_SRC_CLASSH_TGT,
			CS35L45_PCM_SRC_VDD_BATTMON, CS35L45_PCM_SRC_VDD_BSTMON,
			CS35L45_PCM_SRC_TEMPMON};

static const char * const pcm_dac_txt[] = {"Zero", "ASP_RX1", "ASP_RX2",
			"DSP_TX1", "DSP_TX2"};

static const unsigned int pcm_dac_val[] = {CS35L45_PCM_SRC_ZERO,
			CS35L45_PCM_SRC_ASP_RX1, CS35L45_PCM_SRC_ASP_RX2,
			CS35L45_PCM_SRC_DSP_TX1, CS35L45_PCM_SRC_DSP_TX2};

static const char * const pcm_ng_txt[] = {"Zero", "ASP_RX1", "ASP_RX2"};

static const unsigned int pcm_ng_val[] = {CS35L45_PCM_SRC_ZERO,
			CS35L45_PCM_SRC_ASP_RX1, CS35L45_PCM_SRC_ASP_RX2};

static const struct soc_enum mux_enums[] = {
	SOC_VALUE_ENUM_SINGLE(CS35L45_ASPTX1_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_tx_txt), pcm_tx_txt, pcm_tx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_ASPTX2_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_tx_txt), pcm_tx_txt, pcm_tx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_ASPTX3_INPUT, 0, CS35L45_PCM_SRC_MASK,
			 ARRAY_SIZE(pcm_tx_txt), pcm_tx_txt, pcm_tx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_ASPTX4_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_tx_txt), pcm_tx_txt, pcm_tx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_DSP1RX1_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_DSP1RX2_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_DSP1RX3_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_DSP1RX4_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_DSP1RX5_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_DSP1RX6_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_DSP1RX7_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_DSP1RX8_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_rx_txt), pcm_rx_txt, pcm_rx_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_DACPCM1_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_dac_txt), pcm_dac_txt, pcm_dac_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_NGATE1_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_ng_txt), pcm_ng_txt, pcm_ng_val),
	SOC_VALUE_ENUM_SINGLE(CS35L45_NGATE2_INPUT, 0, CS35L45_PCM_SRC_MASK,
			ARRAY_SIZE(pcm_ng_txt), pcm_ng_txt, pcm_ng_val),
};

static const struct snd_kcontrol_new muxes[] = {
	SOC_DAPM_ENUM("ASP_TX1 Source", mux_enums[ASP_TX1]),
	SOC_DAPM_ENUM("ASP_TX2 Source", mux_enums[ASP_TX2]),
	SOC_DAPM_ENUM("ASP_TX3 Source", mux_enums[ASP_TX3]),
	SOC_DAPM_ENUM("ASP_TX4 Source", mux_enums[ASP_TX4]),
};

static const struct snd_kcontrol_new force_en_ctl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new amp_en_ctl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new dsp_en_ctl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new bbpe_en_ctl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new ngate_en_ctl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new nfr_en_ctl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_soc_dapm_widget cs35l45_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("DSP1 Preload", NULL),
	SND_SOC_DAPM_SPK("DSP1", NULL),

	SND_SOC_DAPM_SUPPLY_S("DSP1 Preloader", 100, SND_SOC_NOPM, 0, 0,
			      cs35l45_dsp_loader_ev, SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY_S("DSP1 Boot", 200, SND_SOC_NOPM, 0, 0,
			      cs35l45_dsp_boot_ev, SND_SOC_DAPM_POST_PMU |
			      SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUT_DRV_E("DSP1 Power", SND_SOC_NOPM, 0, 0, NULL, 0,
			       cs35l45_dsp_power_ev, SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_E("GLOBAL_EN", SND_SOC_NOPM, 0, 0, NULL, 0,
			   cs35l45_global_en_ev, SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("Hibernate",  SND_SOC_NOPM, 0, 0,
			    cs35l45_hibernate_dapm, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("VMON", CS35L45_BLOCK_ENABLES, 12, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("IMON", CS35L45_BLOCK_ENABLES, 13, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("BATTMON", CS35L45_BLOCK_ENABLES, 8, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("BSTMON", CS35L45_BLOCK_ENABLES, 9, 0, NULL, 0),

	SND_SOC_DAPM_AIF_IN("ASP", NULL, 0, CS35L45_BLOCK_ENABLES2, 27, 0),
	SND_SOC_DAPM_AIF_IN("ASP_RX1", NULL, 0, CS35L45_ASP_ENABLES1, 16, 0),
	SND_SOC_DAPM_AIF_IN("ASP_RX2", NULL, 0, CS35L45_ASP_ENABLES1, 17, 0),
	SND_SOC_DAPM_AIF_IN("NGATE_CH1", NULL, 0, CS35L45_MIXER_NGATE_CH1_CFG,
			    16, 0),
	SND_SOC_DAPM_AIF_IN("NGATE_CH2", NULL, 0, CS35L45_MIXER_NGATE_CH2_CFG,
			    16, 0),

	SND_SOC_DAPM_AIF_OUT("ASP_TX1", NULL, 0, CS35L45_ASP_ENABLES1, 0, 1),
	SND_SOC_DAPM_AIF_OUT("ASP_TX2", NULL, 0, CS35L45_ASP_ENABLES1, 1, 1),
	SND_SOC_DAPM_AIF_OUT("ASP_TX3", NULL, 0, CS35L45_ASP_ENABLES1, 2, 1),
	SND_SOC_DAPM_AIF_OUT("ASP_TX4", NULL, 0, CS35L45_ASP_ENABLES1, 3, 1),

	SND_SOC_DAPM_MUX("ASP_TX1 Source", SND_SOC_NOPM, 0, 0, &muxes[ASP_TX1]),
	SND_SOC_DAPM_MUX("ASP_TX2 Source", SND_SOC_NOPM, 0, 0, &muxes[ASP_TX2]),
	SND_SOC_DAPM_MUX("ASP_TX3 Source", SND_SOC_NOPM, 0, 0, &muxes[ASP_TX3]),
	SND_SOC_DAPM_MUX("ASP_TX4 Source", SND_SOC_NOPM, 0, 0, &muxes[ASP_TX4]),

	SND_SOC_DAPM_SWITCH("Force Enable", SND_SOC_NOPM, 0, 0, &force_en_ctl),
	SND_SOC_DAPM_SWITCH("AMP Enable", SND_SOC_NOPM, 0, 0, &amp_en_ctl),
	SND_SOC_DAPM_SWITCH("DSP1 Enable", SND_SOC_NOPM, 0, 0, &dsp_en_ctl),
	SND_SOC_DAPM_SWITCH("BBPE Enable", CS35L45_BLOCK_ENABLES2, 13, 0,
			    &bbpe_en_ctl),
	SND_SOC_DAPM_SWITCH("NFR Enable", CS35L45_BLOCK_ENABLES, 1, 0,
			    &nfr_en_ctl),
	SND_SOC_DAPM_SWITCH("NGATE Enable", SND_SOC_NOPM, 0, 0, &ngate_en_ctl),

	SND_SOC_DAPM_MIXER("Exit", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Entry", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("SPK"),
	SND_SOC_DAPM_INPUT("AP"),
};

static const struct snd_soc_dapm_route cs35l45_dapm_routes[] = {
	/* DSP */
	{"DSP1 Preload", NULL, "DSP1 Preloader"},

	{"DSP1", NULL, "DSP1 Boot"},
	{"DSP1", NULL, "DSP1 Power"},

	{"DSP1 Power", NULL, "DSP1 Enable"},
	{"DSP1 Power", NULL, "DSP1 Preloader"},
	{"DSP1 Power", NULL, "DSP1 Boot"},
	{"DSP1 Power", NULL, "VMON"},
	{"DSP1 Power", NULL, "IMON"},
	{"DSP1 Power", NULL, "BATTMON"},
	{"DSP1 Power", NULL, "BSTMON"},

	{"DSP Log DSP", NULL, "DSP1 Power"},

	/* Feedback */
	{"ASP_TX1", NULL, "AP"},
	{"ASP_TX2", NULL, "AP"},
	{"ASP_TX3", NULL, "AP"},
	{"ASP_TX4", NULL, "AP"},

	{"ASP_TX1 Source", "Zero", "ASP_TX1"},
	{"ASP_TX2 Source", "Zero", "ASP_TX2"},
	{"ASP_TX3 Source", "Zero", "ASP_TX3"},
	{"ASP_TX4 Source", "Zero", "ASP_TX4"},

	{"Capture", NULL, "ASP_TX1 Source"},
	{"Capture", NULL, "ASP_TX2 Source"},
	{"Capture", NULL, "ASP_TX3 Source"},
	{"Capture", NULL, "ASP_TX4 Source"},

	{"Capture", NULL, "VMON"},
	{"Capture", NULL, "IMON"},
	{"Capture", NULL, "BATTMON"},
	{"Capture", NULL, "BSTMON"},

	/* Playback */
	{"AMP Enable", "Switch", "Playback"},
	{"DSP1 Enable", "Switch", "Playback"},

	{"Entry", NULL, "AMP Enable"},
	{"Entry", NULL, "DSP1 Enable"},

	{"GLOBAL_EN", NULL, "Entry"},

	{"BBPE Enable", "Switch", "Entry"},
	{"NFR Enable", "Switch", "Entry"},

	{"NGATE_CH1", NULL, "Entry"},
	{"NGATE_CH2", NULL, "Entry"},

	{"ASP_RX1", NULL, "Entry"},
	{"ASP_RX2", NULL, "Entry"},

	{"ASP", NULL, "ASP_RX1"},
	{"ASP", NULL, "ASP_RX2"},

	{"NGATE Enable", "Switch", "NGATE_CH1"},
	{"NGATE Enable", "Switch", "NGATE_CH2"},

	{"Exit", NULL, "ASP"},
	{"Exit", NULL, "BBPE Enable"},
	{"Exit", NULL, "NFR Enable"},
	{"Exit", NULL, "NGATE Enable"},
	{"Exit", NULL, "GLOBAL_EN"},

	{"SPK", NULL, "Exit"},
	{"SPK", NULL, "Hibernate"},
};

static const char * const gain_texts[] = {"10dB", "13dB", "16dB", "19dB"};
static const unsigned int gain_values[] = {0x00, 0x01, 0x02, 0x03};
static SOC_VALUE_ENUM_SINGLE_DECL(gain_enum, CS35L45_AMP_GAIN,
			CS35L45_AMP_GAIN_PCM_SHIFT,
			CS35L45_AMP_GAIN_PCM_MASK >> CS35L45_AMP_GAIN_PCM_SHIFT,
			gain_texts, gain_values);

static const char * const amplifier_mode_texts[] = {"None", "SPK", "RCV"};
static SOC_ENUM_SINGLE_DECL(amplifier_mode_enum, SND_SOC_NOPM, 0,
			    amplifier_mode_texts);

static const DECLARE_TLV_DB_RANGE(dig_pcm_vol_tlv, 0, 0,
				  TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
				  1, 913, TLV_DB_SCALE_ITEM(-10200, 25, 0));

static int cs35l45_amplifier_mode_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l45->amplifier_mode;

	return 0;
}

static int cs35l45_amplifier_mode_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);
	int ret;

	if (ucontrol->value.integer.value[0] == AMP_MODE_NONE) {
		cs35l45->amplifier_mode = ucontrol->value.integer.value[0];

		snd_soc_component_disable_pin(component, "SPK");

		snd_soc_dapm_sync(dapm);

		return 0;
	}

	if (ucontrol->value.integer.value[0] == AMP_MODE_SPK) {
		snd_soc_component_disable_pin(component, "SPK");
		snd_soc_dapm_sync(dapm);

		flush_work(&cs35l45->dsp_pmd_work);

		regmap_update_bits(cs35l45->regmap, CS35L45_BLOCK_ENABLES,
				   CS35L45_RCV_EN_MASK, 0);

		regmap_update_bits(cs35l45->regmap, CS35L45_BLOCK_ENABLES,
				   CS35L45_BST_EN_MASK,
				   CS35L45_BST_ENABLE << CS35L45_BST_EN_SHIFT);

		regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
				   CS35L45_HVLV_MODE_MASK,
				   CS35L45_HVLV_OPERATION <<
				   CS35L45_HVLV_MODE_SHIFT);

		ret = cs35l45_activate_ctl(cs35l45, "AMP PCM Gain", true);
		if (ret < 0)
			dev_info(cs35l45->dev,
				"Unable to deactivate ctl (%d)\n", ret);

		snd_soc_component_enable_pin(component, "SPK");
		snd_soc_dapm_sync(dapm);
	} else  /* AMP_MODE_RCV */ {
		snd_soc_component_disable_pin(component, "SPK");
		snd_soc_dapm_sync(dapm);

		flush_work(&cs35l45->dsp_pmd_work);

		regmap_update_bits(cs35l45->regmap, CS35L45_BLOCK_ENABLES,
				   CS35L45_RCV_EN_MASK, CS35L45_RCV_EN_MASK);

		regmap_update_bits(cs35l45->regmap, CS35L45_BLOCK_ENABLES,
				   CS35L45_BST_EN_MASK,
				   CS35L45_BST_DISABLE_FET_OFF <<
				   CS35L45_BST_EN_SHIFT);

		regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
				   CS35L45_HVLV_MODE_MASK,
				   CS35L45_FORCE_LV_OPERATION <<
				   CS35L45_HVLV_MODE_SHIFT);

		regmap_update_bits(cs35l45->regmap,
				   CS35L45_BLOCK_ENABLES2,
				   CS35L45_AMP_DRE_EN_MASK, 0);

		regmap_update_bits(cs35l45->regmap, CS35L45_AMP_GAIN,
				   CS35L45_AMP_GAIN_PCM_MASK,
				   CS35L45_AMP_GAIN_PCM_13DBV <<
				   CS35L45_AMP_GAIN_PCM_SHIFT);

		ret = cs35l45_activate_ctl(cs35l45, "AMP PCM Gain", false);
		if (ret < 0)
			dev_info(cs35l45->dev,
				"Unable to deactivate ctl (%d)\n", ret);

		snd_soc_component_enable_pin(component, "SPK");
		snd_soc_dapm_sync(dapm);
	}

	cs35l45->amplifier_mode = ucontrol->value.integer.value[0];

	return 0;
}

static int cs35l45_dsp_boot_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	unsigned int val;

	val = snd_soc_component_get_pin_status(component, "DSP1");

	ucontrol->value.integer.value[0] = (val > 0) ? 1 : 0;

	return 0;
}

static int cs35l45_dsp_boot_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);

	if (!cs35l45->dsp.booted) {
		dev_info(cs35l45->dev, "Preload DSP before boot\n");
		return -EPERM;
	}

	if (ucontrol->value.integer.value[0]) {
		snd_soc_component_force_enable_pin(component, "DSP1");

		snd_soc_dapm_sync(dapm);

		regmap_update_bits(cs35l45->regmap, CS35L45_SYNC_TX_RX_ENABLES,
				   CS35L45_SYNC_SW_EN_MASK,
				   CS35L45_SYNC_SW_EN_MASK);
		pm_runtime_put_noidle(cs35l45->dev);
	} else {
		pm_runtime_resume_and_get(cs35l45->dev);

		snd_soc_component_disable_pin(component, "DSP1");

		snd_soc_dapm_sync(dapm);

		regmap_update_bits(cs35l45->regmap, CS35L45_SYNC_TX_RX_ENABLES,
				   CS35L45_SYNC_SW_EN_MASK, 0);
	}

	return 0;
}

static const char *cs35l45_fast_switch_text[] = {
	"fast_switch1.txt",
	"fast_switch2.txt",
	"fast_switch3.txt",
	"fast_switch4.txt",
	"fast_switch5.txt",
};

static int cs35l45_fast_switch_en_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l45->fast_switch_en;

	return 0;
}

static int cs35l45_do_fast_switch(struct cs35l45_private *cs35l45)
{
	char			val_str[CS35L45_BUFSIZE];
	const char		*fw_name;
	const struct firmware	*fw;
	int			ret;
	unsigned int		i, j, k;
	s32			data_ctl_len, val;
	bool			fw_running	= false;
	__be32			*data_ctl_buf, cmd_ctl, st_ctl;

	data_ctl_buf	= NULL;

	fw_name	= cs35l45->fast_switch_names[cs35l45->fast_switch_file_idx];
	dev_dbg(cs35l45->dev, "fw_name:%s\n", fw_name);
	ret	= request_firmware(&fw, fw_name, cs35l45->dev);
	if (ret < 0) {
		dev_info(cs35l45->dev, "Failed to request firmware:%s\n",
			fw_name);
		return -EIO;
	}

	/* Parse number of data in file */
	for (i = 0, j = 0; (char)fw->data[i] != ','; i++) {
		if ((char)fw->data[i] == ' ') {
			/* Skip white space */
		} else {
			/* fw->data[i] must be numerical digit */
			if (j < CS35L45_BUFSIZE - 1) {
				val_str[j]	= fw->data[i];
				j++;
			} else {
				dev_info(cs35l45->dev, "Invalid input\n");
				ret		= -EINVAL;
				goto exit;
			}
		}
	}
	i++;	/* points to beginning of next number */
	val_str[j]	= '\0';
	ret		= kstrtos32(val_str, 10, &data_ctl_len);
	if (ret < 0) {
		dev_info(cs35l45->dev, "kstrtos32 failed (%d) val_str:%s\n",
			ret, val_str);
		goto exit;
	}

	dev_dbg(cs35l45->dev, "data_ctl_len:%u\n", data_ctl_len);

	data_ctl_buf	= kcalloc(1, data_ctl_len * sizeof(__be32), GFP_KERNEL);
	if (!data_ctl_buf) {
		ret	= -ENOMEM;
		goto exit;
	}

	data_ctl_buf[0]	= cpu_to_be32(data_ctl_len);

	/* i continues from end of previous loop */
	for (j = 0, k = 1; i <= fw->size; i++) {
		if (i == fw->size || (char)fw->data[i] == ',') {
			/*
			 * Reached end of parameter
			 * delimited either by ',' or end of file
			 * Parse number and write parameter
			 */
			val_str[j]	= '\0';
			ret		= kstrtos32(val_str, 10, &val);
			if (ret < 0) {
				dev_info(cs35l45->dev,
					"kstrtos32 failed (%d) val_str:%s\n",
					ret, val_str);
				goto exit;
			}
			data_ctl_buf[k] = cpu_to_be32(val);
			j		= 0;
			k++;
		} else if ((char)fw->data[i] == ' ') {
			/* Skip white space */
		} else {
			/* fw->data[i] must be numerical digit */
			if (j < CS35L45_BUFSIZE - 1) {
				val_str[j] = fw->data[i];
				j++;
			} else {
				dev_info(cs35l45->dev, "Invalid input\n");
				ret	= -EINVAL;
				goto exit;
			}
		}
	}

	ret = wm_adsp_write_ctl(&cs35l45->dsp, "CSPL_UPDATE_PARAMS_CONFIG",
				WMFW_ADSP2_YM, CS35L45_ALGID, data_ctl_buf,
				data_ctl_len * sizeof(__be32));
	if (ret < 0) {
		dev_info(cs35l45->dev,
			"Failed to write CSPL_UPDATE_PARAMS_CONFIG\n");
		goto exit;
	}
	dev_dbg(cs35l45->dev,
		"Wrote %u reg for CSPL_UPDATE_PARAMS_CONFIG\n", data_ctl_len);

#ifdef DEBUG
	ret = wm_adsp_read_ctl(&cs35l45->dsp, "CSPL_UPDATE_PARAMS_CONFIG",
			       WMFW_ADSP2_YM, CS35L45_ALGID, data_ctl_buf,
			       data_ctl_len * sizeof(__be32));
	if (ret < 0) {
		dev_info(cs35l45->dev,
			"Failed to read CSPL_UPDATE_PARAMS_CONFIG\n");
		goto exit;
	}
	dev_dbg(cs35l45->dev, "read CSPL_UPDATE_PARAMS_CONFIG:\n");
	for (i = 0; i < data_ctl_len; i++)
		dev_dbg(cs35l45->dev, "%u\n", be32_to_cpu(data_ctl_buf[i]));
#endif
	cmd_ctl		= cpu_to_be32(CSPL_CMD_UPDATE_PARAM);
	ret = wm_adsp_write_ctl(&cs35l45->dsp, "CSPL_COMMAND", WMFW_ADSP2_XM,
				CS35L45_ALGID, &cmd_ctl, sizeof(__be32));
	if (ret < 0) {
		dev_info(cs35l45->dev, "Failed to write CSPL_COMMAND\n");
		goto exit;
	}

	/* Verify CSPL COMMAND */
	for (i = 0; i < 5; i++) {
		ret = wm_adsp_read_ctl(&cs35l45->dsp, "CSPL_STATE",
				       WMFW_ADSP2_XM, CS35L45_ALGID,
				       &st_ctl, sizeof(__be32));
		if (ret < 0) {
			dev_info(cs35l45->dev, "Failed to read CSPL_STATE\n");
			goto exit;
		}
		if (be32_to_cpu(st_ctl) == CSPL_ST_RUNNING) {
			dev_dbg(cs35l45->dev,
				"CSPL STATE == RUNNING (%u attempt)\n", i);
			fw_running	= true;
			break;
		}

		usleep_range(100, 110);
	}

	if (!fw_running) {
		dev_info(cs35l45->dev, "CSPL_STATE (%d) is not running\n",
			st_ctl);
		ret	= -1;
		goto exit;
	}
exit:
	kfree(data_ctl_buf);
	release_firmware(fw);
	return ret;
}

static int cs35l45_fast_switch_en_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int			ret = 0;

	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);

	if (!cs35l45->fast_switch_en && ucontrol->value.integer.value[0])
		/*
		 * Rising on fast switch enable
		 * Perform fast use case switching
		 */
		ret = cs35l45_do_fast_switch(cs35l45);

	cs35l45->fast_switch_en = ucontrol->value.integer.value[0];

	return ret;
}

static int cs35l45_fast_switch_file_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);
	struct soc_enum		*soc_enum;
	unsigned int		i = ucontrol->value.enumerated.item[0];

	soc_enum = (struct soc_enum *)kcontrol->private_value;

	if (i >= soc_enum->items) {
		dev_info(cs35l45->dev, "Invalid mixer input (%u)\n", i);
		return -EINVAL;
	}

	cs35l45->fast_switch_file_idx = i;

	return 0;
}

static int cs35l45_fast_switch_file_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = cs35l45->fast_switch_file_idx;

	return 0;
}

static int cs35l45_get_speaker_status(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l45->speaker_status;

	return 0;
}

static const struct snd_kcontrol_new cs35l45_aud_controls[] = {
	WM_ADSP_FW_CONTROL("DSP1", 0),
	WM_ADSP2_PRELOAD_SWITCH("DSP1", 1),

	SOC_SINGLE("AMP Mute", CS35L45_AMP_OUTPUT_MUTE, 0, 1, 0),
	SOC_SINGLE("SYNC Enable Switch", CS35L45_BLOCK_ENABLES2, 8, 1, 0),
	SOC_SINGLE("PLL Force Enable Switch", CS35L45_REFCLK_INPUT, 16, 1, 0),
	SOC_SINGLE("GLOBAL_EN from GPIO", CS35L45_GLOBAL_ENABLES, 8, 1, 0),
	SOC_SINGLE_EXT("DSP1 Boot Switch", SND_SOC_NOPM, 1, 1, 0,
		       cs35l45_dsp_boot_get, cs35l45_dsp_boot_put),
	SOC_SINGLE_EXT("Fast Use Case Switch Enable", SND_SOC_NOPM, 0, 1, 0,
		       cs35l45_fast_switch_en_get, cs35l45_fast_switch_en_put),
	SOC_SINGLE_EXT("Speaker Open / Short Status", SND_SOC_NOPM, 0,
			SPK_STATUS_SHORT_CIRCUIT, 0,
			cs35l45_get_speaker_status, NULL),
	SOC_SINGLE_RANGE("ASPTX1 Slot Position", CS35L45_ASP_FRAME_CONTROL1, 0,
			 0, 63, 0),
	SOC_SINGLE_RANGE("ASPTX2 Slot Position", CS35L45_ASP_FRAME_CONTROL1, 8,
			 0, 63, 0),
	SOC_SINGLE_RANGE("ASPTX3 Slot Position", CS35L45_ASP_FRAME_CONTROL1, 16,
			 0, 63, 0),
	SOC_SINGLE_RANGE("ASPTX4 Slot Position", CS35L45_ASP_FRAME_CONTROL1, 24,
			 0, 63, 0),
	SOC_SINGLE_RANGE("ASPRX1 Slot Position", CS35L45_ASP_FRAME_CONTROL5, 0,
			 0, 63, 0),
	SOC_SINGLE_RANGE("ASPRX2 Slot Position", CS35L45_ASP_FRAME_CONTROL5, 8,
			 0, 63, 0),

	SOC_ENUM("DSP_RX1 Source", mux_enums[DSP_RX1]),
	SOC_ENUM("DSP_RX2 Source", mux_enums[DSP_RX2]),
	SOC_ENUM("DSP_RX3 Source", mux_enums[DSP_RX3]),
	SOC_ENUM("DSP_RX4 Source", mux_enums[DSP_RX4]),
	SOC_ENUM("DSP_RX5 Source", mux_enums[DSP_RX5]),
	SOC_ENUM("DSP_RX6 Source", mux_enums[DSP_RX6]),
	SOC_ENUM("DSP_RX7 Source", mux_enums[DSP_RX7]),
	SOC_ENUM("DSP_RX8 Source", mux_enums[DSP_RX8]),
	SOC_ENUM("DACPCM Source", mux_enums[DACPCM]),
	SOC_ENUM("NGATE1 Source", mux_enums[NGATE1]),
	SOC_ENUM("NGATE2 Source", mux_enums[NGATE2]),
	SOC_ENUM("AMP PCM Gain", gain_enum),

	SOC_ENUM_EXT("Amplifier Mode", amplifier_mode_enum,
		     cs35l45_amplifier_mode_get, cs35l45_amplifier_mode_put),

	{
		.name = "Digital PCM Volume",
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			  SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.tlv.p  = dig_pcm_vol_tlv,
		.info = snd_soc_info_volsw_sx,
		.get = snd_soc_get_volsw_sx,
		.put = snd_soc_put_volsw_sx,
		.private_value = (unsigned long)&(struct soc_mixer_control)
			{
				 .reg = CS35L45_AMP_PCM_CONTROL,
				 .rreg = CS35L45_AMP_PCM_CONTROL,
				 .shift = 0, .rshift = 0,
				 .max = 0x391, .min = CS35L45_AMP_VOL_PCM_MUTE
			}
	},
};

static int cs35l45_dai_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(codec_dai->component);
	unsigned int asp_fmt, fsync_inv, bclk_inv, master_mode;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		master_mode = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		master_mode = 0;
		break;
	default:
		dev_warn(cs35l45->dev, "Mixed master mode unsupported (%d)\n",
			 fmt & SND_SOC_DAIFMT_MASTER_MASK);
		return -EINVAL;
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_BCLK_MSTR_MASK,
			   master_mode << CS35L45_ASP_BCLK_MSTR_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_FSYNC_MSTR_MASK,
			   master_mode << CS35L45_ASP_FSYNC_MSTR_SHIFT);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		asp_fmt = 0;
		break;
	case SND_SOC_DAIFMT_I2S:
		asp_fmt = 2;
		break;
	default:
		dev_info(cs35l45->dev, "Unsupported DAI format (%d)\n",
			 fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_FMT_MASK,
			   asp_fmt << CS35L45_ASP_FMT_SHIFT);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_IF:
		fsync_inv = 1;
		bclk_inv = 0;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		fsync_inv = 0;
		bclk_inv = 1;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		fsync_inv = 1;
		bclk_inv = 1;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		fsync_inv = 0;
		bclk_inv = 0;
		break;
	default:
		dev_warn(cs35l45->dev, "Invalid clock polarity (%d)\n",
			 fmt & SND_SOC_DAIFMT_INV_MASK);
		return -EINVAL;
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_FSYNC_INV_MASK,
			   fsync_inv << CS35L45_ASP_FSYNC_INV_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
			   CS35L45_ASP_BCLK_INV_MASK,
			   bclk_inv << CS35L45_ASP_BCLK_INV_SHIFT);

	return 0;
}

static int cs35l45_dai_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(dai->component);
	unsigned int asp_width, asp_wl, global_fs;
	struct reg_sequence cs35l45_hpf_override[] = {
		{0x00000040,			0x00000055},
		{0x00000040,			0x000000AA},
		{0x00000044,			0x00000055},
		{0x00000044,			0x000000AA},
		{CS35L45_AMP_PCM_HPF_TST,	CS35l45_HPF_DEFAULT},
		{0x00000040,			0x00000000},
		{0x00000044,			0x00000000},
	};

	/*SET etdm for smartpa*/
	mt_cirrus_tdm_snd_hw_params(substream, params, dai);

	switch (params_rate(params)) {
	case 8000:
		global_fs = CS35L45_8_KHZ;
		break;
	case 16000:
		global_fs = CS35L45_16_KHZ;
		break;
	case 44100:
		cs35l45_hpf_override[4].def = CS35L45_HPF_44P1;
		global_fs = CS35L45_44P100_KHZ;
		break;
	case 48000:
		global_fs = CS35L45_48P0_KHZ;
		break;
	case 88200:
		cs35l45_hpf_override[4].def = CS35L45_HPF_88P2;
		global_fs = CS35L45_88P200_KHZ;
		break;
	case 96000:
		global_fs = CS35L45_96P0_KHZ;
		break;
	default:
		dev_warn(cs35l45->dev, "Unsupported params rate (%d)\n",
			 params_rate(params));
		return -EINVAL;
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_GLOBAL_SAMPLE_RATE,
			   CS35L45_GLOBAL_FS_MASK,
			   global_fs << CS35L45_GLOBAL_FS_SHIFT);

	regmap_multi_reg_write(cs35l45->regmap, cs35l45_hpf_override,
				       ARRAY_SIZE(cs35l45_hpf_override));

	asp_wl = params_width(params);
	if (asp_wl > CS35L45_ASP_WL_MAX)
		asp_wl = CS35L45_ASP_WL_MAX;
	else if (asp_wl < CS35L45_ASP_WL_MIN)
		asp_wl = CS35L45_ASP_WL_MIN;

	asp_width = cs35l45->pdata.use_tdm_slots ?
			    cs35l45->slot_width : params_physical_width(params);

	dev_info(cs35l45->dev, "%s: stream=%d, global_fs=%d, asp_width=%d, asp_wl=%d\n",
			 __func__, substream->stream, global_fs, asp_width, asp_wl);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
				   CS35L45_ASP_WIDTH_RX_MASK,
				   asp_width << CS35L45_ASP_WIDTH_RX_SHIFT);

		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_DATA_CONTROL5,
				   CS35L45_ASP_WL_MASK,
				   asp_wl << CS35L45_ASP_WL_SHIFT);
	} else {
		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL2,
				   CS35L45_ASP_WIDTH_TX_MASK,
				   asp_width << CS35L45_ASP_WIDTH_TX_SHIFT);

		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_DATA_CONTROL1,
				   CS35L45_ASP_WL_MASK,
				   asp_wl << CS35L45_ASP_WL_SHIFT);
	}

	return 0;
}

static int cs35l45_dai_set_tdm_slot(struct snd_soc_dai *dai,
				    unsigned int tx_mask, unsigned int rx_mask,
				    int slots, int slot_width)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(dai->component);

	cs35l45->slot_width = slot_width;

	return 0;
}

static int cs35l45_dai_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(dai->component);

	return cs35l45_set_sysclk(cs35l45, clk_id, freq);
}

static const struct snd_soc_dai_ops cs35l45_dai_ops = {
	.set_fmt = cs35l45_dai_set_fmt,
	.hw_params = cs35l45_dai_hw_params,
	.set_tdm_slot = cs35l45_dai_set_tdm_slot,
	.set_sysclk = cs35l45_dai_set_sysclk,
};

#define CS35L45_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_S24_3LE| \
			 SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S32_LE)

#define CS35L45_RATES (SNDRV_PCM_RATE_8000  | \
		       SNDRV_PCM_RATE_16000 | \
		       SNDRV_PCM_RATE_44100 | \
		       SNDRV_PCM_RATE_48000 | \
		       SNDRV_PCM_RATE_88200 | \
		       SNDRV_PCM_RATE_96000)

static struct snd_soc_dai_driver cs35l45_dai[] = {
	{
		.name = "cs35l45",
		.playback = {
				  .stream_name = "Playback",
				  .channels_min = 1,
				  .channels_max = 8,
				  .rates = CS35L45_RATES,
				  .formats = CS35L45_FORMATS,
		},
		.capture = {
				  .stream_name = "Capture",
				  .channels_min = 1,
				  .channels_max = 8,
				  .rates = CS35L45_RATES,
				  .formats = CS35L45_FORMATS,
		},
		.ops = &cs35l45_dai_ops,
	},
	{
		.name = "cs35l45-cpu-dsplog",
		.capture = {
			.stream_name = "DSP Log CPU",
			.channels_min = 1,
			.channels_max = 1,
			.rates = CS35L45_RATES,
			.formats = CS35L45_FORMATS,
		},
		.compress_new = &snd_soc_new_compress,
	},
	{
		.name = "cs35l45-dsp-dsplog",
		.capture = {
			.stream_name = "DSP Log DSP",
			.channels_min = 1,
			.channels_max = 1,
			.rates = CS35L45_RATES,
			.formats = CS35L45_FORMATS,
		},
	}
};

static int cs35l45_compr_switch(struct wm_adsp *dsp, int cmd)
{
	__be32 cmd_ctl;
	int ret;

	cmd_ctl = cpu_to_be32(cmd);
	ret = wm_adsp_write_ctl(dsp, CS35L45_DSP_LOG_ENABLED, WMFW_ADSP2_XM,
				CS35L45_ALGID_TRACE, &cmd_ctl, sizeof(cmd_ctl));
	if (ret) {
		dev_info(dsp->dev, "Failed to write '%x %s' (%d)\n",
			CS35L45_ALGID_TRACE, CS35L45_DSP_LOG_ENABLED, ret);
		return ret;
	}

	return ret;
}

static void cs35l45_compr_start_work(struct work_struct *work)
{
	struct cs35l45_compr *compr =
		container_of(work, struct cs35l45_compr, start_work);
	struct wm_adsp *dsp = compr->dsp;

	cs35l45_compr_switch(dsp, 1);
}

static void cs35l45_compr_stop_work(struct work_struct *work)
{
	struct cs35l45_compr *compr =
		container_of(work, struct cs35l45_compr, stop_work);
	struct wm_adsp *dsp = compr->dsp;

	cs35l45_compr_switch(dsp, 0);
}

static int cs35l45_compr_open(struct snd_soc_component *component,
					struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 2);
	struct cs35l45_private *cs35l45 = snd_soc_component_get_drvdata(component);
	__be32 buffer_size;
	int ret;

	if (strcmp(codec_dai->name, "cs35l45-dsp-dsplog")) {
		dev_info(cs35l45->dev,
			"No suitable compressed stream for DAI '%s'\n",
			codec_dai->name);
		return -EINVAL;
	}

	mutex_lock(&cs35l45->dsp.pwr_lock);

	if (stream->direction != SND_COMPRESS_CAPTURE) {
		dev_info(cs35l45->dev, "Does not support stream direction\n");
		ret = -EINVAL;
		goto out;
	}

	ret = wm_adsp_read_ctl(&cs35l45->dsp, CS35L45_DSP_LOG_BUFFER_SIZE,
			       WMFW_ADSP2_XM, CS35L45_ALGID_TRACE,
			       &buffer_size, sizeof(buffer_size));
	if (ret) {
		dev_info(cs35l45->dev, "Failed to read '%x %s' (%d)\n",
			CS35L45_ALGID_TRACE, CS35L45_DSP_LOG_BUFFER_SIZE, ret);
		ret = -ENODEV;
		goto out;
	}

	cs35l45->compr = kzalloc(sizeof(*cs35l45->compr), GFP_KERNEL);
	if (!cs35l45->compr) {
		ret = -ENOMEM;
		goto out;
	}

	cs35l45->compr->dsp = &cs35l45->dsp;
	cs35l45->compr->stream = stream;
	cs35l45->compr->buffer_size = be32_to_cpu(buffer_size);
	cs35l45->compr->buffer_count = 0;

	INIT_WORK(&cs35l45->compr->start_work, cs35l45_compr_start_work);
	INIT_WORK(&cs35l45->compr->stop_work, cs35l45_compr_stop_work);

	stream->runtime->private_data = cs35l45->compr;

out:
	mutex_unlock(&cs35l45->dsp.pwr_lock);
	return ret;
}

static int cs35l45_compr_free(struct snd_soc_component *component,
					struct snd_compr_stream *stream)
{
	struct cs35l45_compr *compr = stream->runtime->private_data;
	struct wm_adsp *dsp = compr->dsp;

	flush_scheduled_work();

	cancel_work_sync(&compr->start_work);
	cancel_work_sync(&compr->stop_work);

	mutex_lock(&dsp->pwr_lock);

	kfree(compr->raw_buf);
	kfree(compr);

	mutex_unlock(&dsp->pwr_lock);

	return 0;
}
static int cs35l45_compr_set_params(struct snd_soc_component *component,
					struct snd_compr_stream *stream,
				    struct snd_compr_params *params)
{
	struct cs35l45_compr *compr = stream->runtime->private_data;
	struct wm_adsp *dsp = compr->dsp;
	unsigned int size;

	if (params->buffer.fragment_size % CS35L45_DSP_DATA_WORD_SIZE) {
		dev_info(dsp->dev, "Invalid buffer fragsize=%d fragments=%d\n",
			params->buffer.fragment_size,
			params->buffer.fragments);

		return -EINVAL;
	}

	compr->size = params->buffer;

	dev_dbg(dsp->dev, "fragment_size=%d fragments=%d\n",
		compr->size.fragment_size, compr->size.fragments);

	size = compr->buffer_size * sizeof(*compr->raw_buf);
	compr->raw_buf = kmalloc(size, GFP_DMA | GFP_KERNEL);
	if (!compr->raw_buf)
		return -ENOMEM;

	compr->sample_rate = params->codec.sample_rate;

	return 0;
}
static int cs35l45_compr_get_caps(struct snd_soc_component *component,
				  struct snd_compr_stream *stream,
				  struct snd_compr_caps *caps)
{
	caps->codecs[0] = SND_AUDIOCODEC_BESPOKE;
	caps->num_codecs = 1;
	caps->direction = SND_COMPRESS_CAPTURE;

	return 0;
}

static int cs35l45_compr_trigger(struct snd_soc_component *component,
					struct snd_compr_stream *stream, int cmd)
{
	struct cs35l45_compr *compr = stream->runtime->private_data;
	struct wm_adsp *dsp = compr->dsp;
	int ret = 0;

	dev_dbg(dsp->dev, "Trigger: %d\n", cmd);

	mutex_lock(&dsp->pwr_lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		schedule_work(&compr->start_work);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		schedule_work(&compr->stop_work);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&dsp->pwr_lock);

	return ret;
}

static int cs35l45_buffer_update_avail(struct cs35l45_private *cs35l45)
{
	__be32 addr;
	int read_index;
	int ret;

	ret = wm_adsp_read_ctl(&cs35l45->dsp, CS35L45_DSP_LOG_BUFFER, WMFW_ADSP2_XM,
			       CS35L45_ALGID_TRACE,
			       &addr, sizeof(addr));
	if (ret) {
		dev_info(cs35l45->dev, "Failed to read '%x %s' (%d)\n",
			CS35L45_ALGID_TRACE, CS35L45_DSP_LOG_BUFFER, ret);
		return ret;
	}
	read_index = be32_to_cpu(addr);

	if (read_index == cs35l45->compr->last_read_index) {
		dev_warn(cs35l45->dev, "Avail check on unstarted stream\n");
		return 0;
	}

	cs35l45->compr->read_index = read_index;
	cs35l45->compr->last_read_index = read_index;
	cs35l45->compr->avail = cs35l45->compr->buffer_size
			* CS35L45_DSP_DATA_WORD_SIZE;

	dev_dbg(cs35l45->dev, "readindex=0x%x, avail=%d\n",
		cs35l45->compr->read_index,
		cs35l45->compr->avail);

	return 0;
}

static int cs35l45_compr_pointer(struct snd_soc_component *component,
				 struct snd_compr_stream *stream,
				 struct snd_compr_tstamp *tstamp)
{
	struct cs35l45_compr *compr = stream->runtime->private_data;
	struct wm_adsp *dsp = compr->dsp;
	struct cs35l45_private *cs35l45 =
		container_of(dsp, struct cs35l45_private, dsp);
	int ret = 0;

	dev_dbg(dsp->dev, "Pointer request\n");

	mutex_lock(&dsp->pwr_lock);

	if (dsp->fatal_error) {
		snd_compr_stop_error(stream, SNDRV_PCM_STATE_XRUN);
		ret = -EIO;
		goto out;
	}

	if (compr->buffer_size == 0) {
		ret = cs35l45_buffer_update_avail(cs35l45);
		if (ret < 0) {
			dev_info(dsp->dev, "Error reading avail: %d\n", ret);
			goto out;
		}
	}

	tstamp->copied_total = compr->copied_total;
	tstamp->copied_total += compr->buffer_size * CS35L45_DSP_DATA_WORD_SIZE;
	tstamp->sampling_rate = compr->sample_rate;

out:
	mutex_unlock(&dsp->pwr_lock);

	return ret;
}

static int cs35l45_dsp_read_data_block(struct cs35l45_private *cs35l45,
				       int mem_type, unsigned int mem_addr,
				       unsigned int num_words, u32 *data)
{
	struct wm_adsp *dsp = &cs35l45->dsp;
	struct wm_adsp_region const *mem = NULL;
	unsigned int reg;
	int ret, i;

	for (i = 0; i < dsp->num_mems; i++)
		if (dsp->mem[i].type == mem_type)
			mem = &dsp->mem[i];

	if (!mem)
		return -EINVAL;

	reg = dsp->ops->region_to_reg(mem, mem_addr);

	ret = regmap_raw_read(dsp->regmap, reg, data,
			      sizeof(*data) * num_words);
	if (ret < 0)
		return ret;

	return 0;
}

static void cs35l45_remove_padding(u32 *buf, int nwords)
{
	u8 *pack_in = (u8 *)buf;
	u8 *pack_out = (u8 *)buf;
	u8 temp = pack_in[1];
	int i;

	/* The first interaction will swap position 1 and 2, so use the saved value */
	*pack_out++ = pack_in[3];
	*pack_out++ = pack_in[2]; /* Writing to pack_in[1] */
	*pack_out++ = temp; /* Receive pack_in[1] */
	pack_in += 4;

	/* Remove the padding bytes from the data read from the DSP */
	for (i = 0; i < nwords - 1; i++) {
		*pack_out++ = pack_in[3];
		*pack_out++ = pack_in[2];
		*pack_out++ = pack_in[1];
		pack_in += sizeof(*buf);
	}
}

static int cs35l45_compr_read(struct cs35l45_compr *compr,
			      char __user *buf, size_t count)
{
	struct wm_adsp *dsp = compr->dsp;
	struct cs35l45_private *cs35l45 =
		container_of(dsp, struct cs35l45_private, dsp);
	int ntotal = 0;
	int nwords, nbytes;
	__be32 cmd_ctl;
	int ret;

	dev_dbg(dsp->dev, "Requested read of %zu bytes\n", count);

	if (dsp->fatal_error) {
		snd_compr_stop_error(compr->stream, SNDRV_PCM_STATE_XRUN);
		return -EIO;
	}

	count /= CS35L45_DSP_DATA_WORD_SIZE;

	do {
		nwords = compr->avail;
		if (!nwords)
			return 0;

		nwords = min_t(int, nwords, count);
		if (cs35l45->max_quirks_read_nwords != 0)
			nwords = min(nwords, cs35l45->max_quirks_read_nwords);
		ret = cs35l45_dsp_read_data_block(cs35l45, WMFW_ADSP2_XM,
						  compr->read_index, nwords, compr->raw_buf);
		if (ret) {
			dev_info(dsp->dev, "Failed to read data from DSP (%d)\n", ret);
				return ret;
		}

		cs35l45_remove_padding(compr->raw_buf, nwords);

		nbytes = nwords * CS35L45_DSP_DATA_WORD_SIZE;

		dev_dbg(dsp->dev, "Read %d bytes from compr stream\n", nbytes);

		if (copy_to_user(buf + ntotal, compr->raw_buf, nbytes)) {
			dev_info(dsp->dev, "Failed to copy data to user: %d, %d\n",
				ntotal, nbytes);
			return -EFAULT;
		}

		/* update avail to account for words read */
		compr->avail -= nwords;
		count -= nwords;
		ntotal += nbytes;
		compr->read_index += nwords;
	} while (nwords > 0 && count > 0);

	compr->copied_total += ntotal;

	if (compr->avail <= 0) {
		/* Write ACK to DSP */
		cmd_ctl = cpu_to_be32(1);
		ret = wm_adsp_write_ctl(&cs35l45->dsp,
					CS35L45_DSP_LOG_TRANSFER_COMPLETED,
					WMFW_ADSP2_XM, CS35L45_ALGID_TRACE,
					&cmd_ctl, sizeof(cmd_ctl));
		if (ret) {
			dev_info(cs35l45->dev, "Failed to write '%x %s' (%d)\n",
				CS35L45_ALGID_TRACE, CS35L45_DSP_LOG_TRANSFER_COMPLETED, ret);
			return ret;
		}
	}

	return ntotal;
}

static int cs35l45_compr_copy(struct snd_soc_component *component,
				  struct snd_compr_stream *stream, char __user *buf,
			      size_t count)
{
	struct cs35l45_compr *compr = stream->runtime->private_data;
	struct wm_adsp *dsp = compr->dsp;
	int ret;

	mutex_lock(&dsp->pwr_lock);

	if (stream->direction == SND_COMPRESS_CAPTURE)
		ret = cs35l45_compr_read(compr, buf, count);
	else
		ret = -EOPNOTSUPP;

	mutex_unlock(&dsp->pwr_lock);

	return ret;
}

static const struct snd_compress_ops cs35l145_compr_ops = {
	.open = &cs35l45_compr_open,
	.free = &cs35l45_compr_free,
	.set_params = &cs35l45_compr_set_params,
	.get_caps = &cs35l45_compr_get_caps,
	.trigger = &cs35l45_compr_trigger,
	.pointer = &cs35l45_compr_pointer,
	.copy = &cs35l45_compr_copy,
};

static int cs35l45_component_set_sysclk(struct snd_soc_component *component,
					int clk_id, int source,
					unsigned int freq, int dir)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);

	return cs35l45_set_sysclk(cs35l45, clk_id, freq);
}

static char fast_ctl[] = "Fast Use Case Delta File";
//static char fast_ctl3[] = "Fast Use Case Delta File";

static int cs35l45_component_probe(struct snd_soc_component *component)
{
	struct cs35l45_private *cs35l45 =
			snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);
	int ret = 0;

	dev_dbg(cs35l45->dev,
		"cs35l45_component_probe\n");

	snd_soc_component_disable_pin(component, "DSP1");

	snd_soc_dapm_sync(dapm);

	component->regmap = cs35l45->regmap;

	wm_adsp2_component_probe(&cs35l45->dsp, component);
	cs35l45->component = component;

	/* Add run-time mixer control for fast use case switch */
	cs35l45->fast_ctl.name	= fast_ctl;
	cs35l45->fast_ctl.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	cs35l45->fast_ctl.info	= snd_soc_info_enum_double;
	cs35l45->fast_ctl.get	= cs35l45_fast_switch_file_get;
	cs35l45->fast_ctl.put	= cs35l45_fast_switch_file_put;
	cs35l45->fast_ctl.private_value	=
		(unsigned long)&cs35l45->fast_switch_enum;
	ret = snd_soc_add_component_controls(component, &cs35l45->fast_ctl, 1);

	if (ret < 0)
		dev_info(cs35l45->dev,
			"snd_soc_add_component_controls failed (%d)\n", ret);
	return ret;
}

static void cs35l45_component_remove(struct snd_soc_component *component)
{
	struct cs35l45_private *cs35l45 =
		snd_soc_component_get_drvdata(component);

	wm_adsp2_component_remove(&cs35l45->dsp, component);
}

static const struct snd_soc_component_driver cs35l45_component = {
	.probe = cs35l45_component_probe,
	.remove = cs35l45_component_remove,
	.set_sysclk = cs35l45_component_set_sysclk,

	.dapm_widgets = cs35l45_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cs35l45_dapm_widgets),

	.dapm_routes = cs35l45_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(cs35l45_dapm_routes),

	.controls = cs35l45_aud_controls,
	.num_controls = ARRAY_SIZE(cs35l45_aud_controls),

	.name = DRV_NAME,
	.compress_ops = &cs35l145_compr_ops,
};

static int cs35l45_get_clk_config(int freq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l45_pll_sysclk); i++) {
		if (cs35l45_pll_sysclk[i].freq == freq)
			return cs35l45_pll_sysclk[i].clk_cfg;
	}

	return -EINVAL;
}

static int cs35l45_set_sysclk(struct cs35l45_private *cs35l45, int clk_id,
			      unsigned int freq)
{
	unsigned int val;
	int extclk_cfg, clksrc;

	switch (clk_id) {
	case 0:
		clksrc = CS35L45_PLL_REFCLK_SEL_BCLK;
		break;
	default:
		dev_info(cs35l45->dev, "Invalid CLK Config\n");
		return -EINVAL;
	}

	extclk_cfg = cs35l45_get_clk_config(freq);
	if (extclk_cfg < 0) {
		dev_info(cs35l45->dev, "Invalid CLK Config: %d, freq: %u\n",
			extclk_cfg, freq);
		return -EINVAL;
	}

	regmap_read(cs35l45->regmap, CS35L45_REFCLK_INPUT, &val);
	val = (val & CS35L45_PLL_REFCLK_FREQ_MASK) >>
		     CS35L45_PLL_REFCLK_FREQ_SHIFT;

	if (val == extclk_cfg)
		return 0;

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_OPEN_LOOP_MASK,
			   CS35L45_PLL_OPEN_LOOP_MASK);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_REFCLK_FREQ_MASK,
			   extclk_cfg << CS35L45_PLL_REFCLK_FREQ_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_REFCLK_EN_MASK, 0);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_REFCLK_SEL_MASK,
			   clksrc << CS35L45_PLL_REFCLK_SEL_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_OPEN_LOOP_MASK, 0);

	regmap_update_bits(cs35l45->regmap, CS35L45_REFCLK_INPUT,
			   CS35L45_PLL_REFCLK_EN_MASK,
			   CS35L45_PLL_REFCLK_EN_MASK);

	return 0;
}

static int cs35l45_compr_handle_irq(struct cs35l45_private *cs35l45,
				    unsigned int data)
{
	struct cs35l45_compr *compr;
	int ret = 0;

	mutex_lock(&cs35l45->dsp.pwr_lock);

	if (!cs35l45->compr) {
		ret = -ENODEV;
		goto out;
	}

	compr = cs35l45->compr;

	/* If the received count is the same as the previous one, it indicates that
	 * another MBOX register was written to, which should not be dealt within
	 * this handler.
	 */
	if (data == cs35l45->compr->buffer_count)
		goto out;

	if (data != cs35l45->compr->buffer_count + 1) {
		dev_info(cs35l45->dev, "Buffer count is intermittent: %d", data);
		if (cs35l45->compr->buffer_count == 0)
			dev_info(cs35l45->dev, "Compressed stream is reopened since last IRQ\n");
		else
			dev_info(cs35l45->dev, "Buffer skipped. Last received: %d\n",
				 cs35l45->compr->buffer_count);
	}
	cs35l45->compr->buffer_count = data;

	ret = cs35l45_buffer_update_avail(cs35l45);
	if (ret < 0) {
		dev_info(cs35l45->dev, "Error reading avail: %d\n", ret);
		goto out;
	}

	if (compr->stream)
		snd_compr_fragment_elapsed(compr->stream);

out:
	mutex_unlock(&cs35l45->dsp.pwr_lock);

	return ret;
}

static int cs35l45_dsp_virt2_mbox3_irq_handle(struct cs35l45_private *cs35l45, unsigned int cmd,
					      unsigned int data)
{
	switch (cmd) {
	case EVENT_SPEAKER_STATUS:
		cs35l45->speaker_status = data;
		dev_dbg(cs35l45->dev, "MBOX 3 event detected (SPEAKER_STATUS)\n");
		break;
	case EVENT_BOOT_DONE:
		dev_dbg(cs35l45->dev, "MBOX 3 event detected (BOOT_DONE)\n");
		break;
	default:
		dev_info(cs35l45->dev, "MBOX 3 event not supported %u\n", cmd);
		return -EINVAL;
	}

	return 0;
}

static int cs35l45_dsp_virt2_mbox4_irq_handle(struct cs35l45_private *cs35l45,
					      unsigned int data)
{
	__be32 enabled;
	int ret;

	/* Check if DSP log is enabled */
	ret = wm_adsp_read_ctl(&cs35l45->dsp, CS35L45_DSP_LOG_ENABLED,
			       WMFW_ADSP2_XM, CS35L45_ALGID_TRACE,
			       &enabled, sizeof(enabled));
	if (ret) {
		dev_info(cs35l45->dev,
			"Failed to read control '%x %s' (%d)\n",
			CS35L45_ALGID_TRACE, CS35L45_DSP_LOG_ENABLED, ret);
		return -EINVAL;
	}

	if (be32_to_cpu(enabled) == 1) {
		ret = cs35l45_compr_handle_irq(cs35l45, data);
		if (ret == -ENODEV) {
			dev_info(cs35l45->dev, "Spurious DSP log IRQ\n");
			return -EINVAL;
		}
	} else {
		dev_info(cs35l45->dev, "Spurious DSP log IRQ\n");
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t cs35l45_dsp_virt2_mbox_cb(int irq, void *data)
{
	struct cs35l45_private *cs35l45 = data;
	unsigned int mbox_val;
	int ret = 0;

	dev_dbg(cs35l45->dev, "DSP virtual MBOX 2 write detected!");

	complete(&cs35l45->virt2_mbox_comp);

	ret = regmap_read(cs35l45->regmap, CS35L45_DSP_VIRT2_MBOX_3, &mbox_val);
	if (!ret && mbox_val)
		ret = cs35l45_dsp_virt2_mbox3_irq_handle(cs35l45, mbox_val & CS35L45_MBOX3_CMD_MASK,
				(mbox_val & CS35L45_MBOX3_DATA_MASK) >> CS35L45_MBOX3_DATA_SHIFT);

	/* Handle DSP trace log IRQ */
	ret = regmap_read(cs35l45->regmap, CS35L45_DSP_VIRT2_MBOX_4, &mbox_val);
	if (!ret && mbox_val != 0) {
		ret = cs35l45_dsp_virt2_mbox4_irq_handle(cs35l45, mbox_val);
		if (ret)
			dev_info(cs35l45->dev, "Spurious DSP MBOX4 IRQ\n");
	}

	return IRQ_RETVAL(ret);
}

static irqreturn_t cs35l45_dsp_wdt_expire(int irq, void *data)
{
	struct cs35l45_private *cs35l45 = data;

	dev_info(cs35l45->dev, "DSP Watchdog expired!");

	return IRQ_HANDLED;
}

static irqreturn_t cs35l45_pll_unlock(int irq, void *data)
{
	struct cs35l45_private *cs35l45 = data;

	dev_dbg(cs35l45->dev, "PLL unlock flag rise detected!");

	return IRQ_HANDLED;
}

static irqreturn_t cs35l45_pll_lock(int irq, void *data)
{
	struct cs35l45_private *cs35l45 = data;

	dev_dbg(cs35l45->dev, "PLL lock detected!");

	return IRQ_HANDLED;
}

static void cs35l45_global_err_rls_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct cs35l45_private *cs35l45 =
		container_of(dwork, struct cs35l45_private, global_err_rls_work);

	regmap_update_bits(cs35l45->regmap, CS35L45_ERROR_RELEASE,
			   CS35L45_GLOBAL_ERR_RLS_MASK, 0);

	regmap_update_bits(cs35l45->regmap, CS35L45_ERROR_RELEASE,
			   CS35L45_GLOBAL_ERR_RLS_MASK,
			   CS35L45_GLOBAL_ERR_RLS_MASK);

	regmap_update_bits(cs35l45->regmap, CS35L45_ERROR_RELEASE,
			   CS35L45_GLOBAL_ERR_RLS_MASK, 0);
}

static irqreturn_t cs35l45_global_err(int irq, void *data)
{
	struct cs35l45_private *cs35l45 = data;
	unsigned int val;

	dev_info(cs35l45->dev, "Global error detected!");

	queue_delayed_work(cs35l45->wq, &cs35l45->global_err_rls_work,
			   msecs_to_jiffies(100));

	regmap_read(cs35l45->regmap, CS35L45_IRQ1_EINT_1, &val);

	dev_info(cs35l45->dev, "EINT_1 = 0x%x", val);

	if (val & CS35L45_AMP_SHORT_ERR_MASK) {
		dev_info(cs35l45->dev, "Amplifier short error condition detected\n");
		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_1,
					CS35L45_AMP_SHORT_ERR_MASK);
	}

	if (val & CS35L45_UVLO_VDDBATT_ERR_MASK) {
		dev_info(cs35l45->dev, "VDDBATT undervoltage error condition detected\n");
		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_1,
			     CS35L45_UVLO_VDDBATT_ERR_MASK);
	}

	if (val & CS35L45_BST_SHORT_ERR_MASK) {
		dev_info(cs35l45->dev, "Boost inductor short error condition detected\n");
		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_1,
				CS35L45_BST_SHORT_ERR_MASK);
	}

	if (val & CS35L45_BST_UVP_ERR_MASK) {
		dev_info(cs35l45->dev, "Boost undervoltage error condition detected\n");
		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_1,
			     CS35L45_BST_UVP_ERR_MASK);
	}

	if (val & CS35L45_TEMP_ERR_MASK) {
		dev_info(cs35l45->dev, "Overtemperature error condition detected\n");
		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_1,
			     CS35L45_TEMP_ERR_MASK);
	}

	regmap_read(cs35l45->regmap, CS35L45_IRQ1_EINT_3, &val);

	dev_info(cs35l45->dev, "EINT_3 = 0x%x", val);

	if (val & CS35L45_AMP_CAL_ERR_MASK) {
		dev_info(cs35l45->dev, "Amplifier calibration error condition detected\n");
		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_3,
				CS35L45_AMP_CAL_ERR_MASK);
	}

	regmap_read(cs35l45->regmap, CS35L45_IRQ1_EINT_18, &val);

	dev_info(cs35l45->dev, "EINT_18 = 0x%x", val);

	if (val & CS35L45_UVLO_VDDLV_ERR_MASK) {
		dev_info(cs35l45->dev, "LV threshold detector error condition detected\n");
		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_18,
				 CS35L45_UVLO_VDDLV_ERR_MASK);
	}
	return IRQ_HANDLED;
}

static const struct cs35l45_irq cs35l45_irqs[] = {
	CS35L45_IRQ(DSP_VIRT2_MBOX, "DSP virtual MBOX 2 write flag", cs35l45_dsp_virt2_mbox_cb),
	CS35L45_IRQ(DSP_WDT_EXPIRE, "DSP Watchdog Timer", cs35l45_dsp_wdt_expire),
	CS35L45_IRQ(PLL_UNLOCK_FLAG_RISE, "PLL unlock flag rise", cs35l45_pll_unlock),
	CS35L45_IRQ(PLL_LOCK_FLAG, "PLL lock", cs35l45_pll_lock),
	CS35L45_IRQ(GLOBAL_ERROR, "Global error", cs35l45_global_err),
};

static const struct regmap_irq cs35l45_reg_irqs[] = {
	CS35L45_REG_IRQ(IRQ1_EINT_2, DSP_VIRT2_MBOX),
	CS35L45_REG_IRQ(IRQ1_EINT_2, DSP_WDT_EXPIRE),
	CS35L45_REG_IRQ(IRQ1_EINT_3, PLL_UNLOCK_FLAG_RISE),
	CS35L45_REG_IRQ(IRQ1_EINT_3, PLL_LOCK_FLAG),
	CS35L45_REG_IRQ(IRQ1_EINT_18, GLOBAL_ERROR),
};

static const struct regmap_irq_chip cs35l45_regmap_irq_chip = {
	.name = "cs35l45 IRQ1 Controller",
	.status_base = CS35L45_IRQ1_EINT_1,
	.mask_base = CS35L45_IRQ1_MASK_1,
	.ack_base = CS35L45_IRQ1_EINT_1,
	.num_regs = 18,
	.irqs = cs35l45_reg_irqs,
	.num_irqs = ARRAY_SIZE(cs35l45_reg_irqs),
	.runtime_pm = true,
};

static int cs35l45_gpio_configuration(struct cs35l45_private *cs35l45)
{
	struct cs35l45_platform_data *pdata = &cs35l45->pdata;
	struct gpio_ctrl *gpios[] = {&pdata->gpio_ctrl1, &pdata->gpio_ctrl2,
				     &pdata->gpio_ctrl3};
	unsigned int gpio_regs[] = {CS35L45_GPIO1_CTRL1, CS35L45_GPIO2_CTRL1,
				    CS35L45_GPIO3_CTRL1};
	unsigned int pad_regs[] = {CS35L45_SYNC_GPIO1,
				   CS35L45_INTB_GPIO2_MCLK_REF, CS35L45_GPIO3};
	unsigned int val;
	int i;

	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		if (!gpios[i]->is_present)
			continue;

		if (gpios[i]->dir & CS35L45_VALID_PDATA) {
			val = gpios[i]->dir & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, gpio_regs[i],
					   CS35L45_GPIO_DIR_MASK,
					   val << CS35L45_GPIO_DIR_SHIFT);
		}

		if (gpios[i]->lvl & CS35L45_VALID_PDATA) {
			val = gpios[i]->lvl & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, gpio_regs[i],
					   CS35L45_GPIO_LVL_MASK,
					   val << CS35L45_GPIO_LVL_SHIFT);
		}

		if (gpios[i]->op_cfg & CS35L45_VALID_PDATA) {
			val = gpios[i]->op_cfg & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, gpio_regs[i],
					   CS35L45_GPIO_OP_CFG_MASK,
					   val << CS35L45_GPIO_OP_CFG_SHIFT);
		}

		if (gpios[i]->pol & CS35L45_VALID_PDATA) {
			val = gpios[i]->pol & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, gpio_regs[i],
					   CS35L45_GPIO_POL_MASK,
					   val << CS35L45_GPIO_POL_SHIFT);
		}

		if (gpios[i]->ctrl & CS35L45_VALID_PDATA) {
			val = gpios[i]->ctrl & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, pad_regs[i],
					   CS35L45_GPIO_CTRL_MASK,
					   val << CS35L45_GPIO_CTRL_SHIFT);
		}

		if (gpios[i]->invert & CS35L45_VALID_PDATA) {
			val = gpios[i]->invert & (~CS35L45_VALID_PDATA);
			regmap_update_bits(cs35l45->regmap, pad_regs[i],
					   CS35L45_GPIO_INVERT_MASK,
					   val << CS35L45_GPIO_INVERT_SHIFT);
		}
	}

	return 0;
}

static int cs35l45_apply_of_data(struct cs35l45_private *cs35l45)
{
	struct cs35l45_platform_data *pdata = &cs35l45->pdata;
	const struct of_entry *entry;
	unsigned int val;
	u32 *ptr;
	int i, j, ret;

	if (!pdata)
		return 0;

	if (pdata->asp_sdout_hiz_ctrl & CS35L45_VALID_PDATA) {
		val = pdata->asp_sdout_hiz_ctrl & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_ASP_CONTROL3,
				   CS35L45_ASP_DOUT_HIZ_CTRL_MASK,
				   val << CS35L45_ASP_DOUT_HIZ_CTRL_SHIFT);
	}

	if (pdata->ngate_ch1_hold & CS35L45_VALID_PDATA) {
		val = pdata->ngate_ch1_hold & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_MIXER_NGATE_CH1_CFG,
				   CS35L45_AUX_NGATE_CH_HOLD_MASK,
				   val << CS35L45_AUX_NGATE_CH_HOLD_SHIFT);
	}

	if (pdata->ngate_ch1_thr & CS35L45_VALID_PDATA) {
		val = pdata->ngate_ch1_thr & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_MIXER_NGATE_CH1_CFG,
				   CS35L45_AUX_NGATE_CH_THR_MASK,
				   val << CS35L45_AUX_NGATE_CH_THR_SHIFT);
	}

	if (pdata->ngate_ch2_hold & CS35L45_VALID_PDATA) {
		val = pdata->ngate_ch2_hold & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_MIXER_NGATE_CH2_CFG,
				   CS35L45_AUX_NGATE_CH_HOLD_MASK,
				   val << CS35L45_AUX_NGATE_CH_HOLD_SHIFT);
	}

	if (pdata->ngate_ch2_thr & CS35L45_VALID_PDATA) {
		val = pdata->ngate_ch2_thr & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_MIXER_NGATE_CH2_CFG,
				   CS35L45_AUX_NGATE_CH_THR_MASK,
				   val << CS35L45_AUX_NGATE_CH_THR_SHIFT);
	}

	if (pdata->global_en_gpio & CS35L45_VALID_PDATA) {
		val = pdata->global_en_gpio & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap,
				   CS35L45_GPIO_GLOBAL_ENABLE_CONTROL,
				   CS35L45_GLOB_EN_GPIO_MASK,
				   val << CS35L45_GLOB_EN_GPIO_SHIFT);
	}

	if (!pdata->bst_bpe_inst_cfg.is_present)
		goto bst_bpe_misc_cfg;

	for (i = BST_BPE_INST_THLD; i < BST_BPE_INST_PARAMS; i++) {
		for (j = L0; j < BST_BPE_INST_LEVELS; j++) {
			entry = cs35l45_get_bst_bpe_inst_entry(j, i);
			ptr = cs35l45_get_bst_bpe_inst_param(cs35l45, j, i);
			val = ((*ptr) & (~CS35L45_VALID_PDATA)) << entry->shift;
			if ((entry->reg) && ((*ptr) & CS35L45_VALID_PDATA))
				regmap_update_bits(cs35l45->regmap, entry->reg,
						   entry->mask, val);
		}
	}

bst_bpe_misc_cfg:
	if (!pdata->bst_bpe_misc_cfg.is_present)
		goto bst_bpe_il_lim_cfg;

	for (i = BST_BPE_INST_INF_HOLD_RLS; i < BST_BPE_MISC_PARAMS; i++) {
		ptr = cs35l45_get_bst_bpe_misc_param(cs35l45, i);
		val = ((*ptr) & (~CS35L45_VALID_PDATA))
			<< bst_bpe_misc_map[i].shift;
		if ((*ptr) & CS35L45_VALID_PDATA)
			regmap_update_bits(cs35l45->regmap,
					   bst_bpe_misc_map[i].reg,
					   bst_bpe_misc_map[i].mask, val);
	}

bst_bpe_il_lim_cfg:
	if (!pdata->bst_bpe_il_lim_cfg.is_present)
		goto hvlv_cfg;

	for (i = BST_BPE_IL_LIM_THLD_DEL1; i < BST_BPE_IL_LIM_PARAMS; i++) {
		ptr = cs35l45_get_bst_bpe_il_lim_param(cs35l45, i);
		val = ((*ptr) & (~CS35L45_VALID_PDATA))
			<< bst_bpe_il_lim_map[i].shift;
		if ((*ptr) & CS35L45_VALID_PDATA)
			regmap_update_bits(cs35l45->regmap,
					   bst_bpe_il_lim_map[i].reg,
					   bst_bpe_il_lim_map[i].mask, val);
	}

hvlv_cfg:
	if (!pdata->hvlv_cfg.is_present)
		goto ldpm_cfg;

	if (pdata->hvlv_cfg.hvlv_thld_hys & CS35L45_VALID_PDATA) {
		val = pdata->hvlv_cfg.hvlv_thld_hys & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
				   CS35L45_HVLV_THLD_HYS_MASK,
				   val << CS35L45_HVLV_THLD_HYS_SHIFT);
	}

	if (pdata->hvlv_cfg.hvlv_thld & CS35L45_VALID_PDATA) {
		val = pdata->hvlv_cfg.hvlv_thld & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
				   CS35L45_HVLV_THLD_MASK,
				   val << CS35L45_HVLV_THLD_SHIFT);
	}

	if (pdata->hvlv_cfg.hvlv_dly & CS35L45_VALID_PDATA) {
		val = pdata->hvlv_cfg.hvlv_dly & (~CS35L45_VALID_PDATA);
		regmap_update_bits(cs35l45->regmap, CS35L45_HVLV_CONFIG,
				   CS35L45_HVLV_DLY_MASK,
				   val << CS35L45_HVLV_DLY_SHIFT);
	}

ldpm_cfg:
	if (!pdata->ldpm_cfg.is_present)
		goto classh_cfg;

	for (i = LDPM_GP1_BOOST_SEL; i < LDPM_PARAMS; i++) {
		ptr = cs35l45_get_ldpm_param(cs35l45, i);
		val = ((*ptr) & (~CS35L45_VALID_PDATA)) << ldpm_map[i].shift;
		if ((*ptr) & CS35L45_VALID_PDATA)
			regmap_update_bits(cs35l45->regmap, ldpm_map[i].reg,
					   ldpm_map[i].mask, val);
	}

classh_cfg:
	if (!pdata->classh_cfg.is_present)
		goto gpio_cfg;

	for (i = CH_HDRM; i < CLASSH_PARAMS; i++) {
		ptr = cs35l45_get_classh_param(cs35l45, i);
		val = ((*ptr) & (~CS35L45_VALID_PDATA)) << classh_map[i].shift;
		if ((*ptr) & CS35L45_VALID_PDATA)
			regmap_update_bits(cs35l45->regmap, classh_map[i].reg,
					   classh_map[i].mask, val);
	}

	regmap_update_bits(cs35l45->regmap, CS35L45_CLASSH_CONFIG3,
			   CS35L45_CH_OVB_LATCH_MASK,
			   CS35L45_CH_OVB_LATCH_MASK);

	regmap_update_bits(cs35l45->regmap, CS35L45_CLASSH_CONFIG3,
			   CS35L45_CH_OVB_LATCH_MASK, 0);

gpio_cfg:
	ret = cs35l45_gpio_configuration(cs35l45);
	if (ret < 0) {
		dev_info(cs35l45->dev, "Failed to apply GPIO config (%d)\n",
			ret);
		return ret;
	}

	return 0;
}

static int cs35l45_parse_of_data(struct cs35l45_private *cs35l45)
{
	struct cs35l45_platform_data *pdata = &cs35l45->pdata;
	struct device_node *node = cs35l45->dev->of_node;
	struct device_node *child;
	const struct of_entry *entry;
	struct gpio_ctrl *gpios[] = {&pdata->gpio_ctrl1, &pdata->gpio_ctrl2,
				     &pdata->gpio_ctrl3};
	unsigned int val, num_fast_switch, params[BST_BPE_INST_LEVELS];
	char of_name[32];
	u32 *ptr;
	int ret, i, j;

	if ((!node) || (!pdata))
		return 0;

	ret = of_property_count_strings(node, "cirrus,fast-switch");
	if (ret < 0) {
		/*
		 * device tree do not provide file name.
		 * Use default value
		 */
		num_fast_switch		= ARRAY_SIZE(cs35l45_fast_switch_text);
		cs35l45->fast_switch_enum.items	=
			ARRAY_SIZE(cs35l45_fast_switch_text);
		cs35l45->fast_switch_enum.texts	= cs35l45_fast_switch_text;
		cs35l45->fast_switch_names = cs35l45_fast_switch_text;
	} else {
		/* Device tree provides file name */
		num_fast_switch			= (size_t)ret;
		dev_info(cs35l45->dev, "num_fast_switch:%u\n", num_fast_switch);
		cs35l45->fast_switch_names =
			devm_kmalloc(cs35l45->dev,
				     num_fast_switch * sizeof(char *),
				     GFP_KERNEL);
		if (!cs35l45->fast_switch_names)
			return -ENOMEM;
		of_property_read_string_array(node, "cirrus,fast-switch",
					      cs35l45->fast_switch_names,
					      num_fast_switch);
		for (i = 0; i < num_fast_switch; i++) {
			dev_info(cs35l45->dev, "%d:%s\n", i,
				 cs35l45->fast_switch_names[i]);
		}
		cs35l45->fast_switch_enum.items	= num_fast_switch;
		cs35l45->fast_switch_enum.texts	= cs35l45->fast_switch_names;
	}

	cs35l45->fast_switch_enum.reg		= SND_SOC_NOPM;
	cs35l45->fast_switch_enum.shift_l	= 0;
	cs35l45->fast_switch_enum.shift_r	= 0;
	cs35l45->fast_switch_enum.mask		=
		roundup_pow_of_two(num_fast_switch) - 1;


	ret = of_property_read_u32(node, "cirrus,asp-sdout-hiz-ctrl", &val);
	if (!ret)
		pdata->asp_sdout_hiz_ctrl = val | CS35L45_VALID_PDATA;

	pdata->use_tdm_slots = of_property_read_bool(node,
						     "cirrus,use-tdm-slots");

	ret = of_property_read_string(node, "cirrus,dsp-part-name",
				      &pdata->dsp_part_name);
	if (ret < 0)
		pdata->dsp_part_name = "cs35l45";

	ret = of_property_read_u32(node, "cirrus,ngate-ch1-hold", &val);
	if (!ret)
		pdata->ngate_ch1_hold = val | CS35L45_VALID_PDATA;

	ret = of_property_read_u32(node, "cirrus,ngate-ch1-thr", &val);
	if (!ret)
		pdata->ngate_ch1_thr = val | CS35L45_VALID_PDATA;

	ret = of_property_read_u32(node, "cirrus,ngate-ch2-hold", &val);
	if (!ret)
		pdata->ngate_ch2_hold = val | CS35L45_VALID_PDATA;

	ret = of_property_read_u32(node, "cirrus,ngate-ch2-thr", &val);
	if (!ret)
		pdata->ngate_ch2_thr = val | CS35L45_VALID_PDATA;

	ret = of_property_read_u32(node, "cirrus,global-en-gpio", &val);

	pdata->allow_hibernate = of_property_read_bool(node, "cirrus,allow-hibernate");

	if (!ret)
		pdata->global_en_gpio = val | CS35L45_VALID_PDATA;

	child = of_get_child_by_name(node, "cirrus,bst-bpe-inst-config");
	pdata->bst_bpe_inst_cfg.is_present = child ? true : false;
	if (!pdata->bst_bpe_inst_cfg.is_present)
		goto bst_bpe_misc_cfg;

	for (i = BST_BPE_INST_THLD; i < BST_BPE_INST_PARAMS; i++) {
		entry = cs35l45_get_bst_bpe_inst_entry(L0, i);
		ret = of_property_read_u32_array(child, entry->name, params,
						 BST_BPE_INST_LEVELS);
		if (ret)
			continue;

		for (j = L0; j < BST_BPE_INST_LEVELS; j++) {
			ptr = cs35l45_get_bst_bpe_inst_param(cs35l45, j, i);
			(*ptr) = params[j] | CS35L45_VALID_PDATA;
		}
	}

	of_node_put(child);

bst_bpe_misc_cfg:
	child = of_get_child_by_name(node, "cirrus,bst-bpe-misc-config");
	pdata->bst_bpe_misc_cfg.is_present = child ? true : false;
	if (!pdata->bst_bpe_misc_cfg.is_present)
		goto bst_bpe_il_lim_cfg;

	for (i = BST_BPE_INST_INF_HOLD_RLS; i < BST_BPE_MISC_PARAMS; i++) {
		ptr = cs35l45_get_bst_bpe_misc_param(cs35l45, i);
		ret = of_property_read_u32(child, bst_bpe_misc_map[i].name,
					   &val);
		if (!ret)
			(*ptr) = val | CS35L45_VALID_PDATA;
	}

	of_node_put(child);

bst_bpe_il_lim_cfg:
	child = of_get_child_by_name(node, "cirrus,bst-bpe-il-lim-config");
	pdata->bst_bpe_il_lim_cfg.is_present = child ? true : false;
	if (!pdata->bst_bpe_il_lim_cfg.is_present)
		goto hvlv_cfg;

	for (i = BST_BPE_IL_LIM_THLD_DEL1; i < BST_BPE_IL_LIM_PARAMS; i++) {
		ptr = cs35l45_get_bst_bpe_il_lim_param(cs35l45, i);
		ret = of_property_read_u32(child, bst_bpe_il_lim_map[i].name,
					   &val);
		if (!ret)
			(*ptr) = val | CS35L45_VALID_PDATA;
	}

	of_node_put(child);

hvlv_cfg:
	child = of_get_child_by_name(node, "cirrus,hvlv-config");
	pdata->hvlv_cfg.is_present = child ? true : false;
	if (!pdata->hvlv_cfg.is_present)
		goto ldpm_cfg;

	ret = of_property_read_u32(child, "hvlv-thld-hys", &val);
	if (!ret)
		pdata->hvlv_cfg.hvlv_thld_hys = val | CS35L45_VALID_PDATA;

	ret = of_property_read_u32(child, "hvlv-thld", &val);
	if (!ret)
		pdata->hvlv_cfg.hvlv_thld = val | CS35L45_VALID_PDATA;

	ret = of_property_read_u32(child, "hvlv-dly", &val);
	if (!ret)
		pdata->hvlv_cfg.hvlv_dly = val | CS35L45_VALID_PDATA;

	of_node_put(child);

ldpm_cfg:
	child = of_get_child_by_name(node, "cirrus,ldpm-config");
	pdata->ldpm_cfg.is_present = child ? true : false;
	if (!pdata->ldpm_cfg.is_present)
		goto classh_cfg;

	for (i = LDPM_GP1_BOOST_SEL; i < LDPM_PARAMS; i++) {
		ptr = cs35l45_get_ldpm_param(cs35l45, i);
		ret = of_property_read_u32(child, ldpm_map[i].name, &val);
		if (!ret)
			(*ptr) = val | CS35L45_VALID_PDATA;
	}

	of_node_put(child);

classh_cfg:
	child = of_get_child_by_name(node, "cirrus,classh-config");
	pdata->classh_cfg.is_present = child ? true : false;
	if (!pdata->classh_cfg.is_present)
		goto gpio_cfg;

	for (i = CH_HDRM; i < CLASSH_PARAMS; i++) {
		ptr = cs35l45_get_classh_param(cs35l45, i);
		ret = of_property_read_u32(child, classh_map[i].name, &val);
		if (!ret)
			(*ptr) = val | CS35L45_VALID_PDATA;
	}

	of_node_put(child);

gpio_cfg:
	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		sprintf(of_name, "cirrus,gpio-ctrl%d", i + 1);
		child = of_get_child_by_name(node, of_name);
		gpios[i]->is_present = child ? true : false;
		if (!gpios[i]->is_present)
			continue;

		ret = of_property_read_u32(child, "gpio-dir", &val);
		if (!ret)
			gpios[i]->dir = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-lvl", &val);
		if (!ret)
			gpios[i]->lvl = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-op-cfg", &val);
		if (!ret)
			gpios[i]->op_cfg = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-pol", &val);
		if (!ret)
			gpios[i]->pol = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-ctrl", &val);
		if (!ret)
			gpios[i]->ctrl = val | CS35L45_VALID_PDATA;

		ret = of_property_read_u32(child, "gpio-invert", &val);
		if (!ret)
			gpios[i]->invert = val | CS35L45_VALID_PDATA;

		of_node_put(child);
	}

	return 0;
}

static int cs35l45_activate_ctl(struct cs35l45_private *cs35l45,
				const char *ctl_name, bool active)
{
	struct snd_soc_component *component = cs35l45->component;
	struct snd_card *card = component->card->snd_card;
	struct snd_kcontrol *kcontrol;
	struct snd_kcontrol_volatile *vd;
	unsigned int index_offset;
	char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];

	if (component->name_prefix)
		snprintf(name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN, "%s %s",
			 component->name_prefix, ctl_name);
	else
		snprintf(name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN, "%s", ctl_name);

	kcontrol = snd_soc_card_get_kcontrol(component->card, name);
	if (!kcontrol) {
		dev_info(cs35l45->dev, "Can't find kcontrol %s\n", name);
		return -EINVAL;
	}

	index_offset = snd_ctl_get_ioff(kcontrol, &kcontrol->id);
	vd = &kcontrol->vd[index_offset];
	if (active)
		vd->access |= SNDRV_CTL_ELEM_ACCESS_WRITE;
	else
		vd->access &= ~SNDRV_CTL_ELEM_ACCESS_WRITE;

	snd_ctl_notify(card, SNDRV_CTL_EVENT_MASK_INFO, &kcontrol->id);

	return 0;
}

int cs35l45_suspend_runtime(struct device *dev)
{
	struct cs35l45_private *cs35l45 = dev_get_drvdata(dev);

	if (cs35l45->dsp.booted && cs35l45->pdata.allow_hibernate)
		return cs35l45_hibernate(cs35l45, HIBER_MODE_EN);

	return 0;
}
EXPORT_SYMBOL_GPL(cs35l45_suspend_runtime);

int cs35l45_resume_runtime(struct device *dev)
{
	struct cs35l45_private *cs35l45 = dev_get_drvdata(dev);

	if (cs35l45->dsp.booted && cs35l45->pdata.allow_hibernate)
		return cs35l45_hibernate(cs35l45, HIBER_MODE_DIS);

	return 0;
}
EXPORT_SYMBOL_GPL(cs35l45_resume_runtime);

static int cs35l45_hibernate(struct cs35l45_private *cs35l45, bool hiber_en)
{
	unsigned int sts, cmd, val;
	int ret, i;
	struct cs35l45_mixer_cache mixer_cache[] = {
		{CS35L45_BLOCK_ENABLES, CS35L45_BLOCK_ENABLES_MASK, 0},
		{CS35L45_BLOCK_ENABLES2, CS35L45_SYNC_EN_MASK, 0},
		{CS35L45_SYNC_TX_RX_ENABLES, CS35L45_SYNC_MASK, 0},
		{CS35L45_ASP_ENABLES1, CS35L45_ASP_ENABLES1_MASK, 0},
		{CS35L45_ASPTX1_INPUT,	CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_ASPTX2_INPUT,	CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_ASPTX3_INPUT,	CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_ASPTX4_INPUT,	CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_DSP1RX1_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_DSP1RX2_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_DSP1RX3_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_DSP1RX4_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_DSP1RX5_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_DSP1RX6_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_DSP1RX7_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_DSP1RX8_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_DACPCM1_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_NGATE1_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_NGATE2_INPUT, CS35L45_PCM_SRC_MASK, 0},
		{CS35L45_AMP_GAIN, CS35L45_AMP_GAIN_PCM_MASK, 0},
		{CS35L45_AMP_OUTPUT_MUTE, CS35L45_AMP_MUTE_MASK, 0},
		{CS35L45_AMP_PCM_CONTROL, CS35L45_AMP_VOL_PCM_MASK, 0},
		{CS35L45_ASP_FRAME_CONTROL1, CS35L45_ASP_TX_ALL_SLOTS, 0},
		{CS35L45_ASP_FRAME_CONTROL5, CS35L45_ASP_RX_ALL_SLOTS, 0},
		{CS35L45_REFCLK_INPUT, CS35L45_PLL_FORCE_EN_MASK, 0},
		{CS35L45_IRQ1_MASK_3, 0xffffffff, 0},
		{CS35L45_IRQ1_MASK_18, 0xffffffff, 0},
	};

	if (!cs35l45->dsp.booted) {
		dev_info(cs35l45->dev, "Firmware not loaded\n");
		return -EPERM;
	}

	if (hiber_en == HIBER_MODE_EN) {
		regmap_read(cs35l45->regmap, CS35L45_DSP_MBOX_2, &sts);
		if (((enum cspl_mboxstate)sts) != CSPL_MBOX_STS_PAUSED) {
			dev_info(cs35l45->dev, "FW not paused (%d)\n", sts);
			return -EINVAL;
		}

		flush_work(&cs35l45->dsp_pmd_work);

		regmap_update_bits(cs35l45->regmap, CS35L45_IRQ1_MASK_2,
				   CS35L45_DSP_VIRT2_MBOX_MASK,
				   CS35L45_DSP_VIRT2_MBOX_MASK);

		cmd = CSPL_MBOX_CMD_HIBERNATE;
		regmap_write(cs35l45->regmap, CS35L45_DSP_VIRT1_MBOX_1, cmd);

		ret = cs35l45_activate_ctl(cs35l45, "DSP1 Preload Switch",
					   false);
		if (ret < 0)
			dev_info(cs35l45->dev, "Unable to deactivate ctl (%d)\n",
				ret);

		cs35l45->initialized = false;

		regcache_cache_only(cs35l45->regmap, true);

		dev_info(cs35l45->dev, "Enter into hibernation state\n");
	} else  /* HIBER_MODE_DIS */ {
		for (i = 0; i < ARRAY_SIZE(mixer_cache); i++)
			regmap_read(cs35l45->regmap, mixer_cache[i].reg,
				    &mixer_cache[i].val);
		regcache_cache_only(cs35l45->regmap, false);

		regcache_drop_region(cs35l45->regmap, CS35L45_DEVID,
				     CS35L45_MIXER_NGATE_CH2_CFG);

		for (i = 0; i < 5; i++) {
			usleep_range(200, 300);

			ret = regmap_read(cs35l45->regmap, CS35L45_DEVID, &val);
			if (!ret)
				break;
		}

		if (i == 5) {
			dev_info(cs35l45->dev, "Timeout trying to wake amp");
			return -ETIMEDOUT;
		}

		ret = __cs35l45_initialize(cs35l45);
		if (ret < 0) {
			dev_info(cs35l45->dev, "Failed to reinitialize (%d)\n",
				ret);
			return ret;
		}

		regmap_update_bits(cs35l45->regmap, CS35L45_PWRMGT_CTL,
				   CS35L45_MEM_RDY_MASK, CS35L45_MEM_RDY_MASK);

		usleep_range(100, 200);

		cmd = CSPL_MBOX_CMD_OUT_OF_HIBERNATE;

		regmap_write(cs35l45->regmap, CS35L45_DSP_VIRT1_MBOX_1, cmd);

		usleep_range(1000, 1200);

		regmap_read(cs35l45->regmap, CS35L45_IRQ1_EINT_2, &val);
		if (!(val & CS35L45_DSP_VIRT2_MBOX_MASK))
			dev_info(cs35l45->dev, "Timeout waiting for MBOX ACK\n");

		regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_2,
				   CS35L45_DSP_VIRT2_MBOX_MASK);

		regmap_update_bits(cs35l45->regmap, CS35L45_IRQ1_MASK_2,
				   CS35L45_DSP_VIRT2_MBOX_MASK, 0);

		for (i = 0; i < ARRAY_SIZE(mixer_cache); i++)
			regmap_update_bits(cs35l45->regmap, mixer_cache[i].reg,
					   mixer_cache[i].mask,
					   mixer_cache[i].val);

		ret = cs35l45_activate_ctl(cs35l45, "DSP1 Preload Switch",
					   true);
		if (ret < 0)
			dev_info(cs35l45->dev, "Unable to activate ctl (%d)\n",
				ret);

		dev_info(cs35l45->dev, "Exit from hibernation state\n");
	}

	return 0;
}

static const struct reg_sequence cs35l45_init_patch[] = {
	{0x00000040,		0x00000055},
	{0x00000040,		0x000000AA},
	{0x00000044,		0x00000055},
	{0x00000044,		0x000000AA},
	{0x00006480,		0x0830500A},
	{0x00007C60,		0x1000850B},
	{CS35L45_BOOST_OV_CFG,	0x007000D0},
	{CS35L45_LDPM_CONFIG,	0x0001B636},
	{0x00002C08,		0x00000009},
	{0x00006850,		0x0A30FFC4},
	{0x00003820,		0x00040100},
	{0x00003824,		0x00000000},
	{0x00007CFC,		0x62870004},
	{0x00007C60,		0x1001850B},
	{0x00000040,		0x00000000},
	{0x00000044,		0x00000000},
	{CS35L45_BOOST_CCM_CFG,	0xF0000003},
	{CS35L45_BOOST_DCM_CFG,	0x08710220},
	{CS35L45_ERROR_RELEASE,	0x02200000},
};

static int __cs35l45_initialize(struct cs35l45_private *cs35l45)
{
	struct device *dev = cs35l45->dev;
	unsigned int sts, wksrc;
	int ret;

	if (cs35l45->initialized)
		return -EPERM;

	ret = regmap_read_poll_timeout(cs35l45->regmap, CS35L45_IRQ1_EINT_4, sts,
				       (sts & CS35L45_OTP_BOOT_DONE_STS_MASK), 1000, 5000);
	if (ret < 0) {
		dev_info(cs35l45->dev, "Timeout waiting for OTP boot\n");
		return ret;
	}

	ret = cs35l45_supported_devid(cs35l45);
	if (ret)
		return ret;

	regmap_write(cs35l45->regmap, CS35L45_IRQ1_EINT_4,
		     CS35L45_OTP_BOOT_DONE_STS_MASK | CS35L45_OTP_BUSY_MASK);

	ret = regmap_register_patch(cs35l45->regmap, cs35l45_init_patch,
				    ARRAY_SIZE(cs35l45_init_patch));
	if (ret < 0) {
		dev_info(dev, "Failed to apply init patch %d\n", ret);
		return ret;
	}

	ret = cs35l45_apply_of_data(cs35l45);
	if (ret < 0) {
		dev_info(dev, "applying OF data failed (%d)\n", ret);
		return ret;
	}

	if (cs35l45->bus_type == CONTROL_BUS_I2C)
		wksrc = CS35L45_WKSRC_I2C;
	else
		wksrc = CS35L45_WKSRC_SPI;

	regmap_update_bits(cs35l45->regmap, CS35L45_WAKESRC_CTL,
			   CS35L45_WKSRC_EN_MASK,
			   wksrc << CS35L45_WKSRC_EN_SHIFT);

	regmap_update_bits(cs35l45->regmap, CS35L45_WAKESRC_CTL,
			   CS35L45_UPDT_WKCTL_MASK, CS35L45_UPDT_WKCTL_MASK);

	regmap_update_bits(cs35l45->regmap, CS35L45_WKI2C_CTL,
			   CS35L45_WKI2C_ADDR_MASK, cs35l45->i2c_addr);

	regmap_update_bits(cs35l45->regmap, CS35L45_WKI2C_CTL,
			   CS35L45_UPDT_WKI2C_MASK, CS35L45_UPDT_WKI2C_MASK);

	cs35l45->initialized = true;

	return 0;
}

int cs35l45_initialize(struct cs35l45_private *cs35l45)
{
	struct device *dev = cs35l45->dev;
	unsigned long irq_pol = IRQF_ONESHOT | IRQF_SHARED;
	int ret, i, irq;

	ret = __cs35l45_initialize(cs35l45);
	if (ret < 0) {
		dev_info(dev, "CS35L45 failed to initialize (%d)\n", ret);
		return ret;
	}

	regmap_update_bits(cs35l45->regmap,
			   CS35L45_DSP1_STREAM_ARB_TX1_CONFIG_0,
			   CS35L45_DSP1_STREAM_ARB_TX1_EN_MASK, 0);

	regmap_update_bits(cs35l45->regmap,
			   CS35L45_DSP1_STREAM_ARB_MSTR1_CONFIG_0,
			   CS35L45_DSP1_STREAM_ARB_MSTR0_EN_MASK, 0);

	regmap_update_bits(cs35l45->regmap, CS35L45_DSP1_CCM_CORE_CONTROL,
			   CS35L45_CCM_CORE_EN_MASK, 0);

	if (cs35l45->irq) {
		if (cs35l45->pdata.gpio_ctrl2.invert & (~CS35L45_VALID_PDATA))
			irq_pol |= IRQF_TRIGGER_HIGH;
		else
			irq_pol |= IRQF_TRIGGER_LOW;

		ret = devm_regmap_add_irq_chip(dev, cs35l45->regmap, cs35l45->irq, irq_pol, 0,
					       &cs35l45_regmap_irq_chip, &cs35l45->irq_data);
		if (ret) {
			dev_info(dev, "Failed to register IRQ chip: %d\n", ret);
			return ret;
		}

		for (i = 0; i < ARRAY_SIZE(cs35l45_irqs); i++) {
			irq = regmap_irq_get_virq(cs35l45->irq_data, cs35l45_irqs[i].irq);
			if (irq < 0) {
				dev_info(dev, "Failed to get %s\n", cs35l45_irqs[i].name);
				return irq;
			}

			ret = devm_request_threaded_irq(dev, irq, NULL, cs35l45_irqs[i].handler,
							irq_pol, cs35l45_irqs[i].name, cs35l45);
			if (ret) {
				dev_info(dev, "Failed to request IRQ %s: %d\n",
					cs35l45_irqs[i].name, ret);
				return ret;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cs35l45_initialize);

static const struct reg_sequence cs35l45_fs_errata_patch[] = {
	{0x02B80080,			0x00000001},
	{0x02B80088,			0x00000001},
	{0x02B80090,			0x00000001},
	{0x02B80098,			0x00000001},
	{0x02B800A0,			0x00000001},
	{0x02B800A8,			0x00000001},
	{0x02B800B0,			0x00000001},
	{0x02B800B8,			0x00000001},
	{0x02B80280,			0x00000001},
	{0x02B80288,			0x00000001},
	{0x02B80290,			0x00000001},
	{0x02B80298,			0x00000001},
	{0x02B802A0,			0x00000001},
	{0x02B802A8,			0x00000001},
	{0x02B802B0,			0x00000001},
	{0x02B802B8,			0x00000001},
};

static const struct wm_adsp_region cs35l45_dsp1_regions[] = {
	{ .type = WMFW_HALO_PM_PACKED,	.base = CS35L45_DSP1_PMEM_0 },
	{ .type = WMFW_HALO_XM_PACKED,	.base = CS35L45_DSP1_XMEM_PACK_0 },
	{ .type = WMFW_HALO_YM_PACKED,	.base = CS35L45_DSP1_YMEM_PACK_0 },
	{. type = WMFW_ADSP2_XM,	.base = CS35L45_DSP1_XMEM_UNPACK24_0},
	{. type = WMFW_ADSP2_YM,	.base = CS35L45_DSP1_YMEM_UNPACK24_0},
};

static int cs35l45_dsp_init(struct cs35l45_private *cs35l45)
{
	struct wm_adsp *dsp = &cs35l45->dsp;
	int ret;

	dsp->part = cs35l45->pdata.dsp_part_name;
	dsp->num = 1;
	dsp->type = WMFW_HALO;
	dsp->rev = 0;
	dsp->dev = cs35l45->dev;
	dsp->regmap = cs35l45->regmap;
	dsp->base = CS35L45_DSP1_CLOCK_FREQ;
	dsp->base_sysinfo = CS35L45_DSP1_SYS_ID;
	dsp->mem = cs35l45_dsp1_regions;
	dsp->num_mems = ARRAY_SIZE(cs35l45_dsp1_regions);
	dsp->lock_regions = 0xFFFFFFFF;

	ret = wm_halo_init(dsp);

	regmap_multi_reg_write(cs35l45->regmap, cs35l45_fs_errata_patch,
						   ARRAY_SIZE(cs35l45_fs_errata_patch));

	if (!cs35l45_halo_start_core) {
		cs35l45_halo_start_core = dsp->ops->start_core;
		cs35l45_halo_ops = (*dsp->ops);
		cs35l45_halo_ops.start_core = NULL;
	}

	dsp->ops = &cs35l45_halo_ops;

	return ret;
}

static const char * const cs35l45_supplies[] = {"VA", "VP"};

int cs35l45_probe(struct cs35l45_private *cs35l45)
{
	struct device *dev = cs35l45->dev;
	int ret;
	u32 i;

	BUILD_BUG_ON(ARRAY_SIZE(cs35l45_reg_irqs) < ARRAY_SIZE(cs35l45_reg_irqs));
	BUILD_BUG_ON(ARRAY_SIZE(cs35l45_irqs) != CS35L45_NUM_IRQ);

	cs35l45->fast_switch_en = false;
	cs35l45->fast_switch_file_idx = 0;
	cs35l45->speaker_status = SPK_STATUS_ALL_CLEAR;

	INIT_WORK(&cs35l45->dsp_pmu_work, cs35l45_dsp_pmu_work);
	INIT_WORK(&cs35l45->dsp_pmd_work, cs35l45_dsp_pmd_work);

	INIT_DELAYED_WORK(&cs35l45->global_err_rls_work, cs35l45_global_err_rls_work);

	mutex_init(&cs35l45->dsp_power_lock);

	init_completion(&cs35l45->virt2_mbox_comp);

	for (i = 0; i < ARRAY_SIZE(cs35l45_supplies); i++)
		cs35l45->supplies[i].supply = cs35l45_supplies[i];

	ret = devm_regulator_bulk_get(dev, CS35L45_NUM_SUPPLIES,
				      cs35l45->supplies);
	if (ret < 0) {
		dev_info(dev, "Failed to request core supplies: %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_enable(CS35L45_NUM_SUPPLIES, cs35l45->supplies);
	if (ret < 0) {
		dev_info(dev, "Failed to enable core supplies: %d\n", ret);
		return ret;
	}

	/* returning NULL can be an option if in stereo mode */
	cs35l45->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						      GPIOD_OUT_LOW);
	if (IS_ERR(cs35l45->reset_gpio)) {
		ret = PTR_ERR(cs35l45->reset_gpio);
		cs35l45->reset_gpio = NULL;
		if (ret == -EBUSY) {
			dev_info(dev,
				 "Reset line busy, assuming shared reset\n");
		} else {
			dev_info(dev, "Failed to get reset GPIO: %d\n", ret);
			goto err;
		}
	}

	if (cs35l45->reset_gpio) {
		gpiod_set_value_cansleep(cs35l45->reset_gpio, 0);
		usleep_range(2000, 2100);
		gpiod_set_value_cansleep(cs35l45->reset_gpio, 1);
	}

	cs35l45->slot_width = CS35L45_DEFAULT_SLOT_WIDTH;

	ret = cs35l45_parse_of_data(cs35l45);
	if (ret < 0) {
		dev_info(dev, "parsing OF data failed: %d\n", ret);
		goto err;
	}

	usleep_range(2000, 2100);

	ret = cs35l45_dsp_init(cs35l45);
	if (ret < 0) {
		dev_info(dev, "dsp_init failed: %d\n", ret);
		goto err;
	}

	cs35l45->wq = create_singlethread_workqueue("cs35l45");
	if (cs35l45->wq == NULL) {
		ret = -ENOMEM;
		goto err_dsp;
	}

	cs35l45_pm_runtime_setup(cs35l45);

	return devm_snd_soc_register_component(dev, &cs35l45_component,
					       cs35l45_dai,
					       ARRAY_SIZE(cs35l45_dai));

err_dsp:
	wm_adsp2_remove(&cs35l45->dsp);
err:
	mutex_destroy(&cs35l45->dsp_power_lock);
	regulator_bulk_disable(CS35L45_NUM_SUPPLIES, cs35l45->supplies);
	return ret;
}
EXPORT_SYMBOL_GPL(cs35l45_probe);

int cs35l45_remove(struct cs35l45_private *cs35l45)
{
	if (cs35l45->reset_gpio)
		gpiod_set_value_cansleep(cs35l45->reset_gpio, 0);

	pm_runtime_disable(cs35l45->dev);
	mutex_destroy(&cs35l45->dsp_power_lock);
	destroy_workqueue(cs35l45->wq);
	wm_adsp2_remove(&cs35l45->dsp);
	regulator_bulk_disable(CS35L45_NUM_SUPPLIES, cs35l45->supplies);

	return 0;
}
EXPORT_SYMBOL_GPL(cs35l45_remove);

static void cs35l45_pm_runtime_setup(struct cs35l45_private *cs35l45)
{
	struct device *dev = cs35l45->dev;

	pm_runtime_set_autosuspend_delay(dev, 5000);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
}

MODULE_DESCRIPTION("ASoC CS35L45 driver");
MODULE_AUTHOR("James Schulman, Cirrus Logic Inc, <james.schulman@cirrus.com>");
MODULE_LICENSE("GPL");
