// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/component.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>
#include <linux/of_platform.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <soc/soundwire.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include "internal.h"
#include "rouleur.h"
#include <asoc/wcdcal-hwdep.h>
#include "rouleur-registers.h"
#include "pm2250-spmi.h"
#include <asoc/msm-cdc-pinctrl.h>
#include <dt-bindings/sound/audio-codec-port-types.h>
#include <asoc/msm-cdc-supply.h>
#include <linux/power_supply.h>
#include "asoc/bolero-slave-internal.h"

#define NUM_SWRS_DT_PARAMS 5

#define ROULEUR_VERSION_1_0 1
#define ROULEUR_VERSION_ENTRY_SIZE 32

#define NUM_ATTEMPTS 5
#define SOC_THRESHOLD_LEVEL 25
#define LOW_SOC_MBIAS_REG_MIN_VOLTAGE 2850000

#define FOUNDRY_ID_SEC 0x5

#define ROULEUR_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 |\
			SNDRV_PCM_RATE_384000)
/* Fractional Rates */
#define ROULEUR_FRAC_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 |\
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800)

#define ROULEUR_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

enum {
	CODEC_TX = 0,
	CODEC_RX,
};

enum {
	ALLOW_VPOS_DISABLE,
	HPH_COMP_DELAY,
	HPH_PA_DELAY,
	AMIC2_BCS_ENABLE,
	WCD_SUPPLIES_LPM_MODE,
};

/* TODO: Check on the step values */
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);

static int rouleur_handle_post_irq(void *data);
static int rouleur_reset(struct device *dev, int val);

static const struct regmap_irq ROULEUR_IRQs[ROULEUR_NUM_IRQS] = {
	REGMAP_IRQ_REG(ROULEUR_IRQ_MBHC_BUTTON_PRESS_DET, 0, 0x01),
	REGMAP_IRQ_REG(ROULEUR_IRQ_MBHC_BUTTON_RELEASE_DET, 0, 0x02),
	REGMAP_IRQ_REG(ROULEUR_IRQ_MBHC_ELECT_INS_REM_DET, 0, 0x04),
	REGMAP_IRQ_REG(ROULEUR_IRQ_MBHC_ELECT_INS_REM_LEG_DET, 0, 0x08),
	REGMAP_IRQ_REG(ROULEUR_IRQ_MBHC_SW_DET, 0, 0x10),
	REGMAP_IRQ_REG(ROULEUR_IRQ_HPHR_OCP_INT, 0, 0x20),
	REGMAP_IRQ_REG(ROULEUR_IRQ_HPHR_CNP_INT, 0, 0x40),
	REGMAP_IRQ_REG(ROULEUR_IRQ_HPHL_OCP_INT, 0, 0x80),
	REGMAP_IRQ_REG(ROULEUR_IRQ_HPHL_CNP_INT, 1, 0x01),
	REGMAP_IRQ_REG(ROULEUR_IRQ_EAR_CNP_INT, 1, 0x02),
	REGMAP_IRQ_REG(ROULEUR_IRQ_EAR_OCP_INT, 1, 0x04),
	REGMAP_IRQ_REG(ROULEUR_IRQ_LO_CNP_INT, 1, 0x08),
	REGMAP_IRQ_REG(ROULEUR_IRQ_LO_OCP_INT, 1, 0x10),
	REGMAP_IRQ_REG(ROULEUR_IRQ_HPHL_PDM_WD_INT, 1, 0x20),
	REGMAP_IRQ_REG(ROULEUR_IRQ_HPHR_PDM_WD_INT, 1, 0x40),
	REGMAP_IRQ_REG(ROULEUR_IRQ_HPHL_SURGE_DET_INT, 2, 0x04),
	REGMAP_IRQ_REG(ROULEUR_IRQ_HPHR_SURGE_DET_INT, 2, 0x08),
};

static struct regmap_irq_chip rouleur_regmap_irq_chip = {
	.name = "rouleur",
	.irqs = ROULEUR_IRQs,
	.num_irqs = ARRAY_SIZE(ROULEUR_IRQs),
	.num_regs = 3,
	.status_base = ROULEUR_DIG_SWR_INTR_STATUS_0,
	.mask_base = ROULEUR_DIG_SWR_INTR_MASK_0,
	.ack_base = ROULEUR_DIG_SWR_INTR_CLEAR_0,
	.use_ack = 1,
	.type_base = ROULEUR_DIG_SWR_INTR_LEVEL_0,
	.runtime_pm = false,
	.handle_post_irq = rouleur_handle_post_irq,
	.irq_drv_data = NULL,
};

static struct snd_soc_dai_driver rouleur_dai[] = {
	{
		.name = "rouleur_cdc",
		.playback = {
			.stream_name = "ROULEUR_AIF Playback",
			.rates = ROULEUR_RATES | ROULEUR_FRAC_RATES,
			.formats = ROULEUR_FORMATS,
			.rate_max = 384000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.capture = {
			.stream_name = "ROULEUR_AIF Capture",
			.rates = ROULEUR_RATES,
			.formats = ROULEUR_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
	},
};
static int rouleur_handle_post_irq(void *data)
{
	struct rouleur_priv *rouleur = data;
	u32 status1 = 0, status2 = 0, status3 = 0;

	regmap_read(rouleur->regmap, ROULEUR_DIG_SWR_INTR_STATUS_0, &status1);
	regmap_read(rouleur->regmap, ROULEUR_DIG_SWR_INTR_STATUS_1, &status2);
	regmap_read(rouleur->regmap, ROULEUR_DIG_SWR_INTR_STATUS_2, &status3);

	rouleur->tx_swr_dev->slave_irq_pending =
			((status1 || status2 || status3) ? true : false);

	return IRQ_HANDLED;
}

static int rouleur_init_reg(struct snd_soc_component *component)
{
	/* Disable HPH OCP */
	snd_soc_component_update_bits(component, ROULEUR_ANA_HPHPA_CNP_CTL_2,
					0x03, 0x00);
	/* Enable surge protection */
	snd_soc_component_update_bits(component, ROULEUR_ANA_SURGE_EN,
					0xC0, 0xC0);
	/* Disable mic bias pull down */
	snd_soc_component_update_bits(component, ROULEUR_ANA_MICBIAS_MICB_1_2_EN,
					0x01, 0x00);
	return 0;
}

static int rouleur_set_port_params(struct snd_soc_component *component,
				u8 slv_prt_type, u8 *port_id, u8 *num_ch,
				u8 *ch_mask, u32 *ch_rate,
				u8 *port_type, u8 path)
{
	int i, j;
	u8 num_ports = 0;
	struct codec_port_info (*map)[MAX_PORT][MAX_CH_PER_PORT] = NULL;
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	switch (path) {
	case CODEC_RX:
		map = &rouleur->rx_port_mapping;
		num_ports = rouleur->num_rx_ports;
		break;
	case CODEC_TX:
		map = &rouleur->tx_port_mapping;
		num_ports = rouleur->num_tx_ports;
		break;
	default:
		dev_err(component->dev, "%s Invalid path: %d\n",
			__func__, path);
		return -EINVAL;
	}

	for (i = 0; i <= num_ports; i++) {
		for (j = 0; j < MAX_CH_PER_PORT; j++) {
			if ((*map)[i][j].slave_port_type == slv_prt_type)
				goto found;
		}
	}

	dev_err(component->dev, "%s Failed to find slave port for type %u\n",
					__func__, slv_prt_type);
	return -EINVAL;
found:
	*port_id = i;
	*num_ch = (*map)[i][j].num_ch;
	*ch_mask = (*map)[i][j].ch_mask;
	*ch_rate = (*map)[i][j].ch_rate;
	*port_type = (*map)[i][j].master_port_type;

	return 0;
}

static int rouleur_parse_port_mapping(struct device *dev,
			char *prop, u8 path)
{
	u32 *dt_array, map_size, map_length;
	u32 port_num = 0, ch_mask, ch_rate, old_port_num = 0;
	u32 slave_port_type, master_port_type;
	u32 i, ch_iter = 0;
	int ret = 0;
	u8 *num_ports = NULL;
	struct codec_port_info (*map)[MAX_PORT][MAX_CH_PER_PORT] = NULL;
	struct rouleur_priv *rouleur = dev_get_drvdata(dev);

	switch (path) {
	case CODEC_RX:
		map = &rouleur->rx_port_mapping;
		num_ports = &rouleur->num_rx_ports;
		break;
	case CODEC_TX:
		map = &rouleur->tx_port_mapping;
		num_ports = &rouleur->num_tx_ports;
		break;
	default:
		dev_err(dev, "%s Invalid path: %d\n",
			__func__, path);
		return -EINVAL;
	}

	if (!of_find_property(dev->of_node, prop,
				&map_size)) {
		dev_err(dev, "missing port mapping prop %s\n", prop);
		ret = -EINVAL;
		goto err;
	}

	map_length = map_size / (NUM_SWRS_DT_PARAMS * sizeof(u32));

	dt_array = kzalloc(map_size, GFP_KERNEL);

	if (!dt_array) {
		ret = -ENOMEM;
		goto err;
	}
	ret = of_property_read_u32_array(dev->of_node, prop, dt_array,
				NUM_SWRS_DT_PARAMS * map_length);
	if (ret) {
		dev_err(dev, "%s: Failed to read  port mapping from prop %s\n",
					__func__, prop);
		ret = -EINVAL;
		goto err_pdata_fail;
	}

	for (i = 0; i < map_length; i++) {
		port_num = dt_array[NUM_SWRS_DT_PARAMS * i];
		slave_port_type = dt_array[NUM_SWRS_DT_PARAMS * i + 1];
		ch_mask = dt_array[NUM_SWRS_DT_PARAMS * i + 2];
		ch_rate = dt_array[NUM_SWRS_DT_PARAMS * i + 3];
		master_port_type = dt_array[NUM_SWRS_DT_PARAMS * i + 4];

		if (port_num != old_port_num)
			ch_iter = 0;

		(*map)[port_num][ch_iter].slave_port_type = slave_port_type;
		(*map)[port_num][ch_iter].ch_mask = ch_mask;
		(*map)[port_num][ch_iter].master_port_type = master_port_type;
		(*map)[port_num][ch_iter].num_ch = __sw_hweight8(ch_mask);
		(*map)[port_num][ch_iter++].ch_rate = ch_rate;
		old_port_num = port_num;
	}
	*num_ports = port_num;

err_pdata_fail:
	kfree(dt_array);
err:
	return ret;
}

static int rouleur_tx_connect_port(struct snd_soc_component *component,
					u8 slv_port_type, u8 enable)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	u8 port_id;
	u8 num_ch;
	u8 ch_mask;
	u32 ch_rate;
	u8 port_type;
	u8 num_port = 1;
	int ret = 0;

	ret = rouleur_set_port_params(component, slv_port_type, &port_id,
				&num_ch, &ch_mask, &ch_rate,
				&port_type, CODEC_TX);

	if (ret) {
		dev_err(rouleur->dev, "%s:Failed to set port params: %d\n",
			__func__, ret);
		return ret;
	}

	if (enable)
		ret = swr_connect_port(rouleur->tx_swr_dev, &port_id,
					num_port, &ch_mask, &ch_rate,
					 &num_ch, &port_type);
	else
		ret = swr_disconnect_port(rouleur->tx_swr_dev, &port_id,
					num_port, &ch_mask, &port_type);
	return ret;

}
static int rouleur_rx_connect_port(struct snd_soc_component *component,
					u8 slv_port_type, u8 enable)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	u8 port_id;
	u8 num_ch;
	u8 ch_mask;
	u32 ch_rate;
	u8 port_type;
	u8 num_port = 1;
	int ret = 0;

	ret = rouleur_set_port_params(component, slv_port_type, &port_id,
				&num_ch, &ch_mask, &ch_rate,
				&port_type, CODEC_RX);

	if (ret) {
		dev_err(rouleur->dev, "%s:Failed to set port params: %d\n",
			__func__, ret);
		return ret;
	}

	if (enable)
		ret = swr_connect_port(rouleur->rx_swr_dev, &port_id,
					num_port, &ch_mask, &ch_rate,
					&num_ch, &port_type);
	else
		ret = swr_disconnect_port(rouleur->rx_swr_dev, &port_id,
					num_port, &ch_mask, &port_type);
	return ret;
}

int rouleur_global_mbias_enable(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	mutex_lock(&rouleur->main_bias_lock);
	if (rouleur->mbias_cnt == 0) {
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_MBIAS_EN, 0x20, 0x20);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_MBIAS_EN, 0x10, 0x10);
		usleep_range(1000, 1100);
	}
	rouleur->mbias_cnt++;
	mutex_unlock(&rouleur->main_bias_lock);

	return 0;
}

int rouleur_global_mbias_disable(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	mutex_lock(&rouleur->main_bias_lock);
	if (rouleur->mbias_cnt == 0) {
		dev_dbg(rouleur->dev, "%s:mbias already disabled\n", __func__);
		mutex_unlock(&rouleur->main_bias_lock);
		return 0;
	}
	rouleur->mbias_cnt--;
	if (rouleur->mbias_cnt == 0) {
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_MBIAS_EN, 0x10, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_MBIAS_EN, 0x20, 0x00);
	}
	mutex_unlock(&rouleur->main_bias_lock);

	return 0;
}

static int rouleur_rx_clk_enable(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	mutex_lock(&rouleur->rx_clk_lock);
	if (rouleur->rx_clk_cnt == 0) {
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_CLK_CTL, 0x10, 0x10);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_CLK_CTL, 0x20, 0x20);
		usleep_range(5000, 5100);
		rouleur_global_mbias_enable(component);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_HPHPA_FSM_CLK, 0x7F, 0x11);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_HPHPA_FSM_CLK, 0x80, 0x80);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_NCP_VCTRL, 0x07, 0x06);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_NCP_EN, 0x01, 0x01);
		usleep_range(500, 510);
	}
	rouleur->rx_clk_cnt++;
	mutex_unlock(&rouleur->rx_clk_lock);

	return 0;
}

static int rouleur_rx_clk_disable(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	mutex_lock(&rouleur->rx_clk_lock);
	if (rouleur->rx_clk_cnt == 0) {
		dev_dbg(rouleur->dev, "%s:clk already disabled\n", __func__);
		mutex_unlock(&rouleur->rx_clk_lock);
		return 0;
	}
	rouleur->rx_clk_cnt--;
	if (rouleur->rx_clk_cnt == 0) {
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_HPHPA_FSM_CLK, 0x80, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_HPHPA_FSM_CLK, 0x7F, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_NCP_EN, 0x01, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_CLK_CTL, 0x20, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_CLK_CTL, 0x10, 0x00);
		rouleur_global_mbias_disable(component);

	}
	mutex_unlock(&rouleur->rx_clk_lock);
	return 0;
}

/*
 * rouleur_soc_get_mbhc: get rouleur_mbhc handle of corresponding component
 * @component: handle to snd_soc_component *
 *
 * return rouleur_mbhc handle or error code in case of failure
 */
struct rouleur_mbhc *rouleur_soc_get_mbhc(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur;

	if (!component) {
		pr_err("%s: Invalid params, NULL component\n", __func__);
		return NULL;
	}
	rouleur = snd_soc_component_get_drvdata(component);

	if (!rouleur) {
		pr_err("%s: Invalid params, NULL tavil\n", __func__);
		return NULL;
	}

	return rouleur->mbhc;
}
EXPORT_SYMBOL(rouleur_soc_get_mbhc);

static int rouleur_codec_hphl_dac_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rouleur_rx_clk_enable(component);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_HPHPA_CNP_CTL_1,
				0x02, 0x02);
		snd_soc_component_update_bits(component,
				ROULEUR_SWR_HPHPA_HD2,
				0x38, 0x38);
		set_bit(HPH_COMP_DELAY, &rouleur->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (rouleur->comp1_enable) {
			snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_COMP_CTL_0,
				0x02, 0x02);

			if (rouleur->comp2_enable)
				snd_soc_component_update_bits(component,
					ROULEUR_DIG_SWR_CDC_COMP_CTL_0,
					0x01, 0x01);
			/*
			 * 5ms sleep is required after COMP is enabled as per
			 * HW requirement
			 */
			if (test_bit(HPH_COMP_DELAY, &rouleur->status_mask)) {
				usleep_range(5000, 5100);
				clear_bit(HPH_COMP_DELAY,
					&rouleur->status_mask);
			}
		} else {
			snd_soc_component_update_bits(component,
					ROULEUR_DIG_SWR_CDC_COMP_CTL_0,
					0x02, 0x00);
		}
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX0_CTL,
				0x80, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_GAIN_CTL,
				0x04, 0x04);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_CLK_CTL, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_CLK_CTL,
				0x01, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_GAIN_CTL,
				0x04, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX0_CTL,
				0x80, 0x80);
		if (rouleur->comp1_enable)
			snd_soc_component_update_bits(component,
					ROULEUR_DIG_SWR_CDC_COMP_CTL_0,
					0x02, 0x00);
		break;
	}

	return 0;
}

static int rouleur_codec_hphr_dac_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rouleur_rx_clk_enable(component);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_HPHPA_CNP_CTL_1,
				0x02, 0x02);
		snd_soc_component_update_bits(component,
				ROULEUR_SWR_HPHPA_HD2,
				0x07, 0x07);
		set_bit(HPH_COMP_DELAY, &rouleur->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (rouleur->comp2_enable) {
			snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_COMP_CTL_0,
				0x01, 0x01);

			if (rouleur->comp1_enable)
				snd_soc_component_update_bits(component,
					ROULEUR_DIG_SWR_CDC_COMP_CTL_0,
					0x02, 0x02);
			/*
			 * 5ms sleep is required after COMP is enabled as per
			 * HW requirement
			 */
			if (test_bit(HPH_COMP_DELAY, &rouleur->status_mask)) {
				usleep_range(5000, 5100);
				clear_bit(HPH_COMP_DELAY,
					&rouleur->status_mask);
			}
		} else {
			snd_soc_component_update_bits(component,
					ROULEUR_DIG_SWR_CDC_COMP_CTL_0,
					0x01, 0x00);
		}
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX1_CTL,
				0x80, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_GAIN_CTL,
				0x08, 0x08);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_CLK_CTL, 0x02, 0x02);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			ROULEUR_DIG_SWR_CDC_RX_CLK_CTL, 0x02, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_GAIN_CTL,
				0x08, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX1_CTL,
				0x80, 0x80);
		if (rouleur->comp2_enable)
			snd_soc_component_update_bits(component,
					ROULEUR_DIG_SWR_CDC_COMP_CTL_0,
					0x01, 0x00);
		break;

	}

	return 0;
}

static int rouleur_codec_ear_lo_dac_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rouleur_rx_clk_enable(component);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX0_CTL,
				0x80, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_CLK_CTL,
				0x01, 0x01);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_GAIN_CTL,
				0x04, 0x04);

		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_CLK_CTL,
				0x01, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX_GAIN_CTL,
				0x04, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_RX0_CTL,
				0x80, 0x80);

		break;
	};
	return 0;

}

static int rouleur_codec_enable_hphr_pa(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = swr_slvdev_datapath_control(rouleur->rx_swr_dev,
				    rouleur->rx_swr_dev->dev_num,
				    true);

		set_bit(HPH_PA_DELAY, &rouleur->status_mask);
		usleep_range(200, 210);
		/* Enable HD2 Config for HPHR if foundry id is SEC */
		if (rouleur->foundry_id == FOUNDRY_ID_SEC)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_HPHR_HD2_ENABLE,
						0x04);
		snd_soc_component_update_bits(component,
			ROULEUR_DIG_SWR_PDM_WD_CTL1,
			0x03, 0x03);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 5ms sleep is required after PA is enabled as per
		 * HW requirement.
		 */
		if (test_bit(HPH_PA_DELAY, &rouleur->status_mask)) {
			usleep_range(5000, 5100);
			clear_bit(HPH_PA_DELAY, &rouleur->status_mask);
		}

		if (rouleur->update_wcd_event)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX2 << 0x10));
		wcd_enable_irq(&rouleur->irq_info,
				ROULEUR_IRQ_HPHR_PDM_WD_INT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wcd_disable_irq(&rouleur->irq_info,
				ROULEUR_IRQ_HPHR_PDM_WD_INT);
		if (rouleur->update_wcd_event)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX2 << 0x10 | 0x1));
		blocking_notifier_call_chain(&rouleur->mbhc->notifier,
					     WCD_EVENT_PRE_HPHR_PA_OFF,
					     &rouleur->mbhc->wcd_mbhc);
		set_bit(HPH_PA_DELAY, &rouleur->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 5ms sleep is required after PA is disabled as per
		 * HW requirement.
		 */
		if (test_bit(HPH_PA_DELAY, &rouleur->status_mask)) {

			usleep_range(5000, 5100);
			clear_bit(HPH_PA_DELAY, &rouleur->status_mask);
		}

		if (rouleur->foundry_id == FOUNDRY_ID_SEC)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_HPHR_HD2_ENABLE,
						0x00);
		blocking_notifier_call_chain(&rouleur->mbhc->notifier,
					     WCD_EVENT_POST_HPHR_PA_OFF,
					     &rouleur->mbhc->wcd_mbhc);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_PDM_WD_CTL1,
				0x03, 0x00);
		break;
	};
	return ret;
}

static int rouleur_codec_enable_hphl_pa(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = swr_slvdev_datapath_control(rouleur->rx_swr_dev,
				    rouleur->rx_swr_dev->dev_num,
				    true);
		set_bit(HPH_PA_DELAY, &rouleur->status_mask);
		usleep_range(200, 210);
		if (rouleur->foundry_id == FOUNDRY_ID_SEC)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_HPHL_HD2_ENABLE,
						0x04);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_PDM_WD_CTL0,
				0x03, 0x03);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 5ms sleep is required after PA is enabled as per
		 * HW requirement.
		 */
		if (test_bit(HPH_PA_DELAY, &rouleur->status_mask)) {
			usleep_range(5000, 5100);
			clear_bit(HPH_PA_DELAY, &rouleur->status_mask);
		}

		if (rouleur->update_wcd_event)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10));
		wcd_enable_irq(&rouleur->irq_info,
				ROULEUR_IRQ_HPHL_PDM_WD_INT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wcd_disable_irq(&rouleur->irq_info,
				ROULEUR_IRQ_HPHL_PDM_WD_INT);
		if (rouleur->update_wcd_event)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10 | 0x1));
		blocking_notifier_call_chain(&rouleur->mbhc->notifier,
					     WCD_EVENT_PRE_HPHL_PA_OFF,
					     &rouleur->mbhc->wcd_mbhc);
		set_bit(HPH_PA_DELAY, &rouleur->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 5ms sleep is required after PA is disabled as per
		 * HW requirement.
		 */
		if (test_bit(HPH_PA_DELAY, &rouleur->status_mask)) {
			usleep_range(5000, 5100);
			clear_bit(HPH_PA_DELAY, &rouleur->status_mask);
		}

		if (rouleur->foundry_id == FOUNDRY_ID_SEC)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_HPHL_HD2_ENABLE,
						0x00);
		blocking_notifier_call_chain(&rouleur->mbhc->notifier,
					     WCD_EVENT_POST_HPHL_PA_OFF,
					     &rouleur->mbhc->wcd_mbhc);
		snd_soc_component_update_bits(component,
			ROULEUR_DIG_SWR_PDM_WD_CTL0,
			0x03, 0x00);

		break;
	};
	return ret;
}

static int rouleur_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = swr_slvdev_datapath_control(rouleur->rx_swr_dev,
			    rouleur->rx_swr_dev->dev_num,
			    true);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL_5,
				0x04, 0x00);
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL_4,
				0x0F, 0x0F);
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL,
				0x40, 0x00);
		if (rouleur->foundry_id == FOUNDRY_ID_SEC)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_HPHL_HD2_ENABLE,
						0x04);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_PDM_WD_CTL0,
				0x03, 0x03);
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(5000, 5100);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL_4,
				0x0F, 0x04);
		if (rouleur->update_wcd_event)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10));
		wcd_enable_irq(&rouleur->irq_info,
				ROULEUR_IRQ_HPHL_PDM_WD_INT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wcd_disable_irq(&rouleur->irq_info,
				ROULEUR_IRQ_HPHL_PDM_WD_INT);
		if (rouleur->update_wcd_event)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10 | 0x1));
		break;
	case SND_SOC_DAPM_POST_PMD:
		usleep_range(5000, 5100);
		if (rouleur->foundry_id == FOUNDRY_ID_SEC)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_HPHL_HD2_ENABLE,
						0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_PDM_WD_CTL0,
				0x03, 0x00);
	};
	return ret;
}

static int rouleur_codec_enable_lo_pa(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = swr_slvdev_datapath_control(rouleur->rx_swr_dev,
			    rouleur->rx_swr_dev->dev_num,
			    true);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL_5,
				0x04, 0x00);
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL_4,
				0x0F, 0x0F);
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL,
				0x40, 0x40);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_PDM_WD_CTL0,
				0x03, 0x03);
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(5000, 5100);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL_4,
				0x0F, 0x04);
		if (rouleur->update_wcd_event)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10));
		wcd_enable_irq(&rouleur->irq_info,
				ROULEUR_IRQ_HPHL_PDM_WD_INT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wcd_disable_irq(&rouleur->irq_info,
					ROULEUR_IRQ_HPHL_PDM_WD_INT);
		if (rouleur->update_wcd_event)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10 | 0x1));
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL,
				0x40, 0x00);
		usleep_range(5000, 5100);
		snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_PDM_WD_CTL0,
				0x03, 0x00);
	};
	return ret;
}

static int rouleur_enable_rx1(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rouleur_rx_connect_port(component, HPH_L, true);
		if (rouleur->comp1_enable)
			rouleur_rx_connect_port(component, COMP_L, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		rouleur_rx_connect_port(component, HPH_L, false);
		if (rouleur->comp1_enable)
			rouleur_rx_connect_port(component, COMP_L, false);
		rouleur_rx_clk_disable(component);
		break;
	};
	return 0;
}

static int rouleur_enable_rx2(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rouleur_rx_connect_port(component, HPH_R, true);
		if (rouleur->comp2_enable)
			rouleur_rx_connect_port(component, COMP_R, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		rouleur_rx_connect_port(component, HPH_R, false);
		if (rouleur->comp2_enable)
			rouleur_rx_connect_port(component, COMP_R, false);
		rouleur_rx_clk_disable(component);
		break;
	};

	return 0;
}

static int rouleur_codec_enable_dmic(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
	unsigned int dmic;
	char *wname;
	int ret = 0;

	wname = strpbrk(w->name, "01");

	if (!wname) {
		dev_err(component->dev, "%s: widget not found\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(wname, 10, &dmic);
	if (ret < 0) {
		dev_err(component->dev, "%s: Invalid DMIC line on the codec\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (dmic) {
	case 0:
	case 1:
		dmic_clk_cnt = &(rouleur->dmic_0_1_clk_cnt);
		dmic_clk_reg = ROULEUR_DIG_SWR_CDC_DMIC1_CTL;
		break;
	default:
		dev_err(component->dev, "%s: Invalid DMIC Selection\n",
			__func__);
		return -EINVAL;
	};
	dev_dbg(component->dev, "%s: event %d DMIC%d dmic_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			ROULEUR_DIG_SWR_CDC_AMIC_CTL, 0x02, 0x00);
		snd_soc_component_update_bits(component,
			dmic_clk_reg, 0x08, 0x08);
		rouleur_tx_connect_port(component, DMIC0 + (w->shift), true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		rouleur_tx_connect_port(component, DMIC0 + (w->shift), false);
		snd_soc_component_update_bits(component,
			dmic_clk_reg, 0x08, 0x00);
		snd_soc_component_update_bits(component,
			ROULEUR_DIG_SWR_CDC_AMIC_CTL, 0x02, 0x02);
		break;

	};
	return 0;
}

static int rouleur_tx_swr_ctrl(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = swr_slvdev_datapath_control(rouleur->tx_swr_dev,
		    rouleur->tx_swr_dev->dev_num,
		    true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = swr_slvdev_datapath_control(rouleur->tx_swr_dev,
		    rouleur->tx_swr_dev->dev_num,
		    false);
		break;
	};

	return ret;
}

static int rouleur_codec_enable_adc(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur =
			snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enable BCS for Headset mic */
		if (w->shift == 1 && !(snd_soc_component_read32(component,
				ROULEUR_ANA_TX_AMIC2) & 0x10)) {
			rouleur_tx_connect_port(component, MBHC, true);
			set_bit(AMIC2_BCS_ENABLE, &rouleur->status_mask);
		}
		rouleur_tx_connect_port(component, ADC1 + (w->shift), true);
		rouleur_global_mbias_enable(component);
		if (w->shift)
			snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_TX_ANA_MODE_0_1,
				0x30, 0x30);
		else
			snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_TX_ANA_MODE_0_1,
				0x03, 0x03);
		break;
	case SND_SOC_DAPM_POST_PMD:
		rouleur_tx_connect_port(component, ADC1 + (w->shift), false);
		if (w->shift == 1 &&
			test_bit(AMIC2_BCS_ENABLE, &rouleur->status_mask)) {
			rouleur_tx_connect_port(component, MBHC, false);
			clear_bit(AMIC2_BCS_ENABLE, &rouleur->status_mask);
		}
		if (w->shift)
			snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_TX_ANA_MODE_0_1,
				0x30, 0x00);
		else
			snd_soc_component_update_bits(component,
				ROULEUR_DIG_SWR_CDC_TX_ANA_MODE_0_1,
				0x03, 0x00);
		rouleur_global_mbias_disable(component);
		break;
	};

	return 0;
}

/*
 * rouleur_get_micb_vout_ctl_val: converts micbias from volts to register value
 * @micb_mv: micbias in mv
 *
 * return register value converted
 */
int rouleur_get_micb_vout_ctl_val(u32 micb_mv)
{
	/* min micbias voltage is 1.6V and maximum is 2.85V */
	if (micb_mv < 1600 || micb_mv > 2850) {
		pr_err("%s: unsupported micbias voltage\n", __func__);
		return -EINVAL;
	}

	return (micb_mv - 1600) / 50;
}
EXPORT_SYMBOL(rouleur_get_micb_vout_ctl_val);

/*
 * rouleur_mbhc_micb_adjust_voltage: adjust specific micbias voltage
 * @component: handle to snd_soc_component *
 * @req_volt: micbias voltage to be set
 * @micb_num: micbias to be set, e.g. micbias1 or micbias2
 *
 * return 0 if adjustment is success or error code in case of failure
 */
int rouleur_mbhc_micb_adjust_voltage(struct snd_soc_component *component,
				   int req_volt, int micb_num)
{
	struct rouleur_priv *rouleur =
			snd_soc_component_get_drvdata(component);
	int cur_vout_ctl, req_vout_ctl;
	int micb_reg, micb_val, micb_en;
	int ret = 0;
	int pullup_mask;

	micb_reg = ROULEUR_ANA_MICBIAS_MICB_1_2_EN;
	switch (micb_num) {
	case MIC_BIAS_1:
		micb_val = snd_soc_component_read32(component, micb_reg);
		micb_en = (micb_val & 0x40) >> 6;
		pullup_mask = 0x20;
		break;
	case MIC_BIAS_2:
		micb_val = snd_soc_component_read32(component, micb_reg);
		micb_en = (micb_val & 0x04) >> 2;
		pullup_mask = 0x02;
		break;
	case MIC_BIAS_3:
	default:
		dev_err(component->dev, "%s: Invalid micbias number: %d\n",
			__func__, micb_num);
		return -EINVAL;
	}
	mutex_lock(&rouleur->micb_lock);

	/*
	 * If requested micbias voltage is same as current micbias
	 * voltage, then just return. Otherwise, adjust voltage as
	 * per requested value. If micbias is already enabled, then
	 * to avoid slow micbias ramp-up or down enable pull-up
	 * momentarily, change the micbias value and then re-enable
	 * micbias.
	 */
	cur_vout_ctl = (snd_soc_component_read32(component,
				ROULEUR_ANA_MICBIAS_LDO_1_SETTING)) & 0xF8;
	cur_vout_ctl = cur_vout_ctl >> 3;
	req_vout_ctl = rouleur_get_micb_vout_ctl_val(req_volt);
	if (req_vout_ctl < 0) {
		ret = -EINVAL;
		goto exit;
	}
	if (cur_vout_ctl == req_vout_ctl) {
		ret = 0;
		goto exit;
	}

	dev_dbg(component->dev, "%s: micb_num: %d, cur_mv: %d, req_mv: %d, micb_en: %d\n",
		 __func__, micb_num, WCD_VOUT_CTL_TO_MICB(cur_vout_ctl),
		 req_volt, micb_en);

	if (micb_en == 0x1)
		snd_soc_component_update_bits(component, micb_reg, pullup_mask,
					      pullup_mask);

	snd_soc_component_update_bits(component,
		ROULEUR_ANA_MICBIAS_LDO_1_SETTING, 0xF8, req_vout_ctl << 3);

	if (micb_en == 0x1) {
		snd_soc_component_update_bits(component, micb_reg,
					      pullup_mask, 0x00);
		/*
		 * Add 2ms delay as per HW requirement after enabling
		 * micbias
		 */
		usleep_range(2000, 2100);
	}
exit:
	mutex_unlock(&rouleur->micb_lock);
	return ret;
}
EXPORT_SYMBOL(rouleur_mbhc_micb_adjust_voltage);

int rouleur_micbias_control(struct snd_soc_component *component,
				int micb_num, int req, bool is_dapm)
{

	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	int micb_index = micb_num - 1;
	u16 micb_reg;
	int pre_off_event = 0, post_off_event = 0;
	int post_on_event = 0, post_dapm_off = 0;
	int post_dapm_on = 0;
	u8 pullup_mask = 0, enable_mask = 0;
	int ret = 0;

	if ((micb_index < 0) || (micb_index > ROULEUR_MAX_MICBIAS - 1)) {
		dev_err(component->dev, "%s: Invalid micbias index, micb_ind:%d\n",
			__func__, micb_index);
		return -EINVAL;
	}
	switch (micb_num) {
	case MIC_BIAS_1:
		micb_reg = ROULEUR_ANA_MICBIAS_MICB_1_2_EN;
		pullup_mask = 0x20;
		enable_mask = 0x40;
		break;
	case MIC_BIAS_2:
		micb_reg = ROULEUR_ANA_MICBIAS_MICB_1_2_EN;
		pullup_mask = 0x02;
		enable_mask = 0x04;
		pre_off_event = WCD_EVENT_PRE_MICBIAS_2_OFF;
		post_off_event = WCD_EVENT_POST_MICBIAS_2_OFF;
		post_on_event = WCD_EVENT_POST_MICBIAS_2_ON;
		post_dapm_on = WCD_EVENT_POST_DAPM_MICBIAS_2_ON;
		post_dapm_off = WCD_EVENT_POST_DAPM_MICBIAS_2_OFF;
		break;
	case MIC_BIAS_3:
		micb_reg = ROULEUR_ANA_MICBIAS_MICB_3_EN;
		pullup_mask = 0x02;
		break;
	default:
		dev_err(component->dev, "%s: Invalid micbias number: %d\n",
			__func__, micb_num);
		return -EINVAL;
	};
	mutex_lock(&rouleur->micb_lock);

	switch (req) {
	case MICB_PULLUP_ENABLE:
		if (!rouleur->dev_up) {
			dev_dbg(component->dev, "%s: enable req %d wcd device down\n",
				__func__, req);
			ret = -ENODEV;
			goto done;
		}
		rouleur->pullup_ref[micb_index]++;
		if ((rouleur->pullup_ref[micb_index] == 1) &&
		    (rouleur->micb_ref[micb_index] == 0))
			snd_soc_component_update_bits(component, micb_reg,
				pullup_mask, pullup_mask);
		break;
	case MICB_PULLUP_DISABLE:
		if (!rouleur->dev_up) {
			dev_dbg(component->dev, "%s: enable req %d wcd device down\n",
				__func__, req);
			ret = -ENODEV;
			goto done;
		}
		if (rouleur->pullup_ref[micb_index] > 0)
			rouleur->pullup_ref[micb_index]--;
		if ((rouleur->pullup_ref[micb_index] == 0) &&
		    (rouleur->micb_ref[micb_index] == 0))
			snd_soc_component_update_bits(component, micb_reg,
				pullup_mask, 0x00);
		break;
	case MICB_ENABLE:
		if (!rouleur->dev_up) {
			dev_dbg(component->dev, "%s: enable req %d wcd device down\n",
				__func__, req);
			ret = -ENODEV;
			goto done;
		}
		rouleur->micb_ref[micb_index]++;
		if (rouleur->micb_ref[micb_index] == 1) {
			rouleur_global_mbias_enable(component);
			snd_soc_component_update_bits(component,
				micb_reg, enable_mask, enable_mask);
			if (post_on_event)
				blocking_notifier_call_chain(
					&rouleur->mbhc->notifier, post_on_event,
					&rouleur->mbhc->wcd_mbhc);
		}
		if (is_dapm && post_dapm_on && rouleur->mbhc)
			blocking_notifier_call_chain(
				&rouleur->mbhc->notifier, post_dapm_on,
				&rouleur->mbhc->wcd_mbhc);
		break;
	case MICB_DISABLE:
		if (rouleur->micb_ref[micb_index] > 0)
			rouleur->micb_ref[micb_index]--;
		if (!rouleur->dev_up) {
			dev_dbg(component->dev, "%s: enable req %d wcd device down\n",
				__func__, req);
			ret = -ENODEV;
			goto done;
		}
		if ((rouleur->micb_ref[micb_index] == 0) &&
		    (rouleur->pullup_ref[micb_index] > 0)) {
			snd_soc_component_update_bits(component, micb_reg,
				pullup_mask, pullup_mask);
                        snd_soc_component_update_bits(component, micb_reg,
                                enable_mask, 0x00);
			rouleur_global_mbias_disable(component);
		} else if ((rouleur->micb_ref[micb_index] == 0) &&
			   (rouleur->pullup_ref[micb_index] == 0)) {
			if (pre_off_event && rouleur->mbhc)
				blocking_notifier_call_chain(
					&rouleur->mbhc->notifier, pre_off_event,
					&rouleur->mbhc->wcd_mbhc);
                        snd_soc_component_update_bits(component, micb_reg,
                                enable_mask, 0x00);
			rouleur_global_mbias_disable(component);
			if (post_off_event && rouleur->mbhc)
				blocking_notifier_call_chain(
					&rouleur->mbhc->notifier,
					post_off_event,
					&rouleur->mbhc->wcd_mbhc);
		}
		if (is_dapm && post_dapm_off && rouleur->mbhc)
			blocking_notifier_call_chain(
				&rouleur->mbhc->notifier, post_dapm_off,
				&rouleur->mbhc->wcd_mbhc);
		break;
	};

	dev_dbg(component->dev, "%s: micb_num:%d, micb_ref: %d, pullup_ref: %d\n",
		__func__, micb_num, rouleur->micb_ref[micb_index],
		rouleur->pullup_ref[micb_index]);
done:
	mutex_unlock(&rouleur->micb_lock);
	return 0;
}
EXPORT_SYMBOL(rouleur_micbias_control);

void rouleur_disable_bcs_before_slow_insert(struct snd_soc_component *component,
					    bool bcs_disable)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	if (rouleur->update_wcd_event) {
		if (bcs_disable)
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_BCS_CLK_OFF, 0);
		else
			rouleur->update_wcd_event(rouleur->handle,
						SLV_BOLERO_EVT_BCS_CLK_OFF, 1);
	}
}

static int rouleur_get_logical_addr(struct swr_device *swr_dev)
{
	int ret = 0;
	uint8_t devnum = 0;
	int num_retry = NUM_ATTEMPTS;

	do {
		ret = swr_get_logical_dev_num(swr_dev, swr_dev->addr, &devnum);
		if (ret) {
			dev_err(&swr_dev->dev,
				"%s get devnum %d for dev addr %lx failed\n",
				__func__, devnum, swr_dev->addr);
			/* retry after 1ms */
			usleep_range(1000, 1010);
		}
	} while (ret && --num_retry);
	swr_dev->dev_num = devnum;
	return 0;
}

static bool get_usbc_hs_status(struct snd_soc_component *component,
			       struct wcd_mbhc_config *mbhc_cfg)
{
	if (mbhc_cfg->enable_usbc_analog) {
		if (!(snd_soc_component_read32(component, ROULEUR_ANA_MBHC_MECH)
			& 0x20))
			return true;
	}
	return false;
}

static int rouleur_event_notify(struct notifier_block *block,
				unsigned long val,
				void *data)
{
	u16 event = (val & 0xffff);
	int ret = 0;
	struct rouleur_priv *rouleur = dev_get_drvdata((struct device *)data);
	struct snd_soc_component *component = rouleur->component;
	struct wcd_mbhc *mbhc;

	switch (event) {
	case BOLERO_SLV_EVT_PA_OFF_PRE_SSR:
		snd_soc_component_update_bits(component,
					ROULEUR_ANA_HPHPA_CNP_CTL_2,
					0xC0, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL,
				0x40, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL,
				0x80, 0x00);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL,
				0x40, 0x40);
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_COMBOPA_CTL,
				0x80, 0x00);
		break;
	case BOLERO_SLV_EVT_SSR_DOWN:
		rouleur->dev_up = false;
		rouleur->mbhc->wcd_mbhc.deinit_in_progress = true;
		mbhc = &rouleur->mbhc->wcd_mbhc;
		rouleur->usbc_hs_status = get_usbc_hs_status(component,
						mbhc->mbhc_cfg);
		rouleur_mbhc_ssr_down(rouleur->mbhc, component);
		rouleur_reset(rouleur->dev, 0x01);
		break;
	case BOLERO_SLV_EVT_SSR_UP:
		rouleur_reset(rouleur->dev, 0x00);
		/* allow reset to take effect */
		usleep_range(10000, 10010);
		rouleur_get_logical_addr(rouleur->tx_swr_dev);
		rouleur_get_logical_addr(rouleur->rx_swr_dev);

		rouleur_init_reg(component);
		regcache_mark_dirty(rouleur->regmap);
		regcache_sync(rouleur->regmap);
		rouleur->dev_up = true;
		/* Initialize MBHC module */
		mbhc = &rouleur->mbhc->wcd_mbhc;
		ret = rouleur_mbhc_post_ssr_init(rouleur->mbhc, component);
		if (ret) {
			dev_err(component->dev, "%s: mbhc initialization failed\n",
				__func__);
		} else {
			rouleur_mbhc_hs_detect(component, mbhc->mbhc_cfg);
			if (rouleur->usbc_hs_status)
				mdelay(500);
		}
		rouleur->mbhc->wcd_mbhc.deinit_in_progress = false;
		break;
	default:
		dev_err(component->dev, "%s: invalid event %d\n", __func__,
			event);
		break;
	}
	return 0;
}

static int __rouleur_codec_enable_micbias(struct snd_soc_dapm_widget *w,
					  int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	int micb_num;

	dev_dbg(component->dev, "%s: wname: %s, event: %d\n",
		__func__, w->name, event);

	if (strnstr(w->name, "MIC BIAS1", sizeof("MIC BIAS1")))
		micb_num = MIC_BIAS_1;
	else if (strnstr(w->name, "MIC BIAS2", sizeof("MIC BIAS2")))
		micb_num = MIC_BIAS_2;
	else if (strnstr(w->name, "MIC BIAS3", sizeof("MIC BIAS3")))
		micb_num = MIC_BIAS_3;
	else
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Micbias LD0 enable not supported for MicBias 3*/
		if (micb_num == MIC_BIAS_3)
			rouleur_micbias_control(component, micb_num,
				MICB_PULLUP_ENABLE, true);
		else
			rouleur_micbias_control(component, micb_num,
				MICB_ENABLE, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(1000, 1100);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (micb_num == MIC_BIAS_3)
			rouleur_micbias_control(component, micb_num,
				MICB_PULLUP_DISABLE, true);
		else
			rouleur_micbias_control(component, micb_num,
				MICB_DISABLE, true);
		break;
	};

	return 0;

}

static int rouleur_codec_enable_micbias(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	return __rouleur_codec_enable_micbias(w, event);
}

static int __rouleur_codec_enable_micbias_pullup(struct snd_soc_dapm_widget *w,
						 int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	int micb_num;

	dev_dbg(component->dev, "%s: wname: %s, event: %d\n",
		__func__, w->name, event);

	if (strnstr(w->name, "VA MIC BIAS1", sizeof("VA MIC BIAS1")))
		micb_num = MIC_BIAS_1;
	else if (strnstr(w->name, "VA MIC BIAS2", sizeof("VA MIC BIAS2")))
		micb_num = MIC_BIAS_2;
	else if (strnstr(w->name, "VA MIC BIAS3", sizeof("VA MIC BIAS3")))
		micb_num = MIC_BIAS_3;
	else
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rouleur_micbias_control(component, micb_num,
					MICB_PULLUP_ENABLE, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* 1 msec delay as per HW requirement */
		usleep_range(1000, 1100);
		break;
	case SND_SOC_DAPM_POST_PMD:
		rouleur_micbias_control(component, micb_num,
					MICB_PULLUP_DISABLE, true);
		break;
	};

	return 0;

}

static int rouleur_codec_enable_micbias_pullup(struct snd_soc_dapm_widget *w,
					       struct snd_kcontrol *kcontrol,
					       int event)
{
	return __rouleur_codec_enable_micbias_pullup(w, event);
}

static int rouleur_get_compander(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	bool hphr;
	struct soc_multi_mixer_control *mc;

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);
	hphr = mc->shift;

	ucontrol->value.integer.value[0] = hphr ? rouleur->comp2_enable :
						rouleur->comp1_enable;
	return 0;
}

static int rouleur_set_compander(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	int value = ucontrol->value.integer.value[0];
	bool hphr;
	struct soc_multi_mixer_control *mc;

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);
	hphr = mc->shift;
	if (hphr)
		rouleur->comp2_enable = value;
	else
		rouleur->comp1_enable = value;

	return 0;
}

static int rouleur_codec_enable_pa_vpos(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	struct rouleur_pdata *pdata = NULL;
	int ret = 0;

	pdata = dev_get_platdata(rouleur->dev);

	if (!pdata) {
		dev_err(component->dev, "%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (test_bit(ALLOW_VPOS_DISABLE, &rouleur->status_mask)) {
			dev_dbg(component->dev,
				"%s: vpos already in enabled state\n",
				__func__);
			clear_bit(ALLOW_VPOS_DISABLE, &rouleur->status_mask);
			return 0;
		}
		ret = msm_cdc_enable_ondemand_supply(rouleur->dev,
						rouleur->supplies,
						pdata->regulator,
						pdata->num_supplies,
						"cdc-pa-vpos");
		if (ret == -EINVAL) {
			dev_err(component->dev, "%s: pa vpos is not enabled\n",
				__func__);
			return ret;
		}
		clear_bit(ALLOW_VPOS_DISABLE, &rouleur->status_mask);
		/*
		 * 200us sleep is required after LDO15 is enabled as per
		 * HW requirement
		 */
		usleep_range(200, 250);

		break;
	case SND_SOC_DAPM_POST_PMD:
		set_bit(ALLOW_VPOS_DISABLE, &rouleur->status_mask);
		ret = swr_slvdev_datapath_control(rouleur->rx_swr_dev,
				rouleur->rx_swr_dev->dev_num,
				false);
		break;
	}
	return 0;
}

static const struct snd_kcontrol_new rouleur_snd_controls[] = {
	SOC_SINGLE_EXT("HPHL_COMP Switch", SND_SOC_NOPM, 0, 1, 0,
		rouleur_get_compander, rouleur_set_compander),
	SOC_SINGLE_EXT("HPHR_COMP Switch", SND_SOC_NOPM, 1, 1, 0,
		rouleur_get_compander, rouleur_set_compander),

	SOC_SINGLE_TLV("HPHL Volume", ROULEUR_ANA_HPHPA_L_GAIN, 0, 20, 1,
					line_gain),
	SOC_SINGLE_TLV("HPHR Volume", ROULEUR_ANA_HPHPA_R_GAIN, 0, 20, 1,
					line_gain),
	SOC_SINGLE_TLV("ADC1 Volume", ROULEUR_ANA_TX_AMIC1, 0, 8, 0,
			analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", ROULEUR_ANA_TX_AMIC2, 0, 8, 0,
			analog_gain),
};

static const struct snd_kcontrol_new adc1_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new adc2_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic1_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic2_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new ear_rdac_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new lo_rdac_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new hphl_rdac_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new hphr_rdac_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const char * const adc2_mux_text[] = {
	"INP2", "INP3"
};

static const struct soc_enum adc2_enum =
	SOC_ENUM_SINGLE(ROULEUR_ANA_TX_AMIC2, 4,
		ARRAY_SIZE(adc2_mux_text), adc2_mux_text);


static const struct snd_kcontrol_new tx_adc2_mux =
	SOC_DAPM_ENUM("ADC2 MUX Mux", adc2_enum);


static const struct snd_soc_dapm_widget rouleur_dapm_widgets[] = {

	/*input widgets*/
	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_INPUT("IN1_HPHL"),
	SND_SOC_DAPM_INPUT("IN2_HPHR"),

	/*tx widgets*/
	SND_SOC_DAPM_ADC_E("ADC1", NULL, SND_SOC_NOPM, 0, 0,
				rouleur_codec_enable_adc,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2", NULL, SND_SOC_NOPM, 1, 0,
				rouleur_codec_enable_adc,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0,
				&tx_adc2_mux),

	/*tx mixers*/
	SND_SOC_DAPM_MIXER_E("ADC1_MIXER", SND_SOC_NOPM, 0, 0,
				adc1_switch, ARRAY_SIZE(adc1_switch),
				rouleur_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("ADC2_MIXER", SND_SOC_NOPM, 0, 0,
				adc2_switch, ARRAY_SIZE(adc2_switch),
				rouleur_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),

	/* micbias widgets*/
	SND_SOC_DAPM_SUPPLY("MIC BIAS1", SND_SOC_NOPM, 0, 0,
				rouleur_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("MIC BIAS2", SND_SOC_NOPM, 0, 0,
				rouleur_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("MIC BIAS3", SND_SOC_NOPM, 0, 0,
				rouleur_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("PA_VPOS", SND_SOC_NOPM, 0, 0,
			     rouleur_codec_enable_pa_vpos,
			     SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/*rx widgets*/
	SND_SOC_DAPM_PGA_E("EAR PGA", ROULEUR_ANA_COMBOPA_CTL, 7, 0, NULL, 0,
				rouleur_codec_enable_ear_pa,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LO PGA", ROULEUR_ANA_COMBOPA_CTL, 7, 0, NULL, 0,
				rouleur_codec_enable_lo_pa,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHL PGA", ROULEUR_ANA_HPHPA_CNP_CTL_2, 7, 0, NULL,
				0, rouleur_codec_enable_hphl_pa,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHR PGA", ROULEUR_ANA_HPHPA_CNP_CTL_2, 6, 0, NULL,
				0, rouleur_codec_enable_hphr_pa,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("RDAC1", NULL, SND_SOC_NOPM, 0, 0,
				rouleur_codec_hphl_dac_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RDAC2", NULL, SND_SOC_NOPM, 0, 0,
				rouleur_codec_hphr_dac_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RDAC3", NULL, SND_SOC_NOPM, 0, 0,
				rouleur_codec_ear_lo_dac_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX1", SND_SOC_NOPM, 0, 0, NULL, 0,
				rouleur_enable_rx1, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2", SND_SOC_NOPM, 0, 0, NULL, 0,
				rouleur_enable_rx2, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),

	/* rx mixer widgets*/

	SND_SOC_DAPM_MIXER("EAR_RDAC", SND_SOC_NOPM, 0, 0,
			   ear_rdac_switch, ARRAY_SIZE(ear_rdac_switch)),
	SND_SOC_DAPM_MIXER("LO_RDAC", SND_SOC_NOPM, 0, 0,
			   lo_rdac_switch, ARRAY_SIZE(lo_rdac_switch)),
	SND_SOC_DAPM_MIXER("HPHL_RDAC", SND_SOC_NOPM, 0, 0,
			   hphl_rdac_switch, ARRAY_SIZE(hphl_rdac_switch)),
	SND_SOC_DAPM_MIXER("HPHR_RDAC", SND_SOC_NOPM, 0, 0,
			   hphr_rdac_switch, ARRAY_SIZE(hphr_rdac_switch)),

	/*output widgets tx*/

	SND_SOC_DAPM_OUTPUT("ADC1_OUTPUT"),
	SND_SOC_DAPM_OUTPUT("ADC2_OUTPUT"),

	/*output widgets rx*/
	SND_SOC_DAPM_OUTPUT("EAR"),
	SND_SOC_DAPM_OUTPUT("LO"),
	SND_SOC_DAPM_OUTPUT("HPHL"),
	SND_SOC_DAPM_OUTPUT("HPHR"),

	/* micbias pull up widgets*/
	SND_SOC_DAPM_SUPPLY("VA MIC BIAS1", SND_SOC_NOPM, 0, 0,
				rouleur_codec_enable_micbias_pullup,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("VA MIC BIAS2", SND_SOC_NOPM, 0, 0,
				rouleur_codec_enable_micbias_pullup,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("VA MIC BIAS3", SND_SOC_NOPM, 0, 0,
				rouleur_codec_enable_micbias_pullup,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
				rouleur_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 1, 0,
				rouleur_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/*tx mixer widgets*/
	SND_SOC_DAPM_MIXER_E("DMIC1_MIXER", SND_SOC_NOPM, 0,
				0, dmic1_switch, ARRAY_SIZE(dmic1_switch),
				rouleur_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("DMIC2_MIXER", SND_SOC_NOPM, 0,
				0, dmic2_switch, ARRAY_SIZE(dmic2_switch),
				rouleur_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),

	/*output widgets*/
	SND_SOC_DAPM_OUTPUT("DMIC1_OUTPUT"),
	SND_SOC_DAPM_OUTPUT("DMIC2_OUTPUT"),
};

static const struct snd_soc_dapm_route rouleur_audio_map[] = {
	{"ADC1_OUTPUT", NULL, "ADC1_MIXER"},
	{"ADC1_MIXER", "Switch", "ADC1"},
	{"ADC1", NULL, "AMIC1"},

	{"ADC2_OUTPUT", NULL, "ADC2_MIXER"},
	{"ADC2_MIXER", "Switch", "ADC2"},
	{"ADC2", NULL, "ADC2 MUX"},
	{"ADC2 MUX", "INP3", "AMIC3"},
	{"ADC2 MUX", "INP2", "AMIC2"},

	{"IN1_HPHL", NULL, "PA_VPOS"},
	{"RX1", NULL, "IN1_HPHL"},
	{"RDAC1", NULL, "RX1"},
	{"HPHL_RDAC", "Switch", "RDAC1"},
	{"HPHL PGA", NULL, "HPHL_RDAC"},
	{"HPHL", NULL, "HPHL PGA"},

	{"IN2_HPHR", NULL, "PA_VPOS"},
	{"RX2", NULL, "IN2_HPHR"},
	{"RDAC2", NULL, "RX2"},
	{"HPHR_RDAC", "Switch", "RDAC2"},
	{"HPHR PGA", NULL, "HPHR_RDAC"},
	{"HPHR", NULL, "HPHR PGA"},

	{"RDAC3", NULL, "RX1"},
	{"EAR_RDAC", "Switch", "RDAC3"},
	{"EAR PGA", NULL, "EAR_RDAC"},
	{"EAR", NULL, "EAR PGA"},

	{"RDAC3", NULL, "RX1"},
	{"LO_RDAC", "Switch", "RDAC3"},
	{"LO PGA", NULL, "LO_RDAC"},
	{"LO", NULL, "LO PGA"},

	{"DMIC1_OUTPUT", NULL, "DMIC1_MIXER"},
	{"DMIC1_MIXER", "Switch", "DMIC1"},

	{"DMIC2_OUTPUT", NULL, "DMIC2_MIXER"},
	{"DMIC2_MIXER", "Switch", "DMIC2"},
};

static ssize_t rouleur_version_read(struct snd_info_entry *entry,
				   void *file_private_data,
				   struct file *file,
				   char __user *buf, size_t count,
				   loff_t pos)
{
	struct rouleur_priv *priv;
	char buffer[ROULEUR_VERSION_ENTRY_SIZE];
	int len = 0;

	priv = (struct rouleur_priv *) entry->private_data;
	if (!priv) {
		pr_err("%s: rouleur priv is null\n", __func__);
		return -EINVAL;
	}

	switch (priv->version) {
	case ROULEUR_VERSION_1_0:
		len = snprintf(buffer, sizeof(buffer), "ROULEUR_1_0\n");
		break;
	default:
		len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops rouleur_info_ops = {
	.read = rouleur_version_read,
};

/*
 * rouleur_info_create_codec_entry - creates rouleur module
 * @codec_root: The parent directory
 * @component: component instance
 *
 * Creates rouleur module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int rouleur_info_create_codec_entry(struct snd_info_entry *codec_root,
				   struct snd_soc_component *component)
{
	struct snd_info_entry *version_entry;
	struct rouleur_priv *priv;
	struct snd_soc_card *card;

	if (!codec_root || !component)
		return -EINVAL;

	priv = snd_soc_component_get_drvdata(component);
	if (priv->entry) {
		dev_dbg(priv->dev,
			"%s:rouleur module already created\n", __func__);
		return 0;
	}
	card = component->card;
	priv->entry = snd_info_create_module_entry(codec_root->module,
					     "rouleur", codec_root);
	if (!priv->entry) {
		dev_dbg(component->dev, "%s: failed to create rouleur entry\n",
			__func__);
		return -ENOMEM;
	}
	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   priv->entry);
	if (!version_entry) {
		dev_dbg(component->dev, "%s: failed to create rouleur version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = priv;
	version_entry->size = ROULEUR_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &rouleur_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	priv->version_entry = version_entry;

	return 0;
}
EXPORT_SYMBOL(rouleur_info_create_codec_entry);

static int rouleur_set_micbias_data(struct rouleur_priv *rouleur,
			      struct rouleur_pdata *pdata)
{
	int vout_ctl = 0;
	int rc = 0;

	if (!pdata) {
		dev_err(rouleur->dev, "%s: NULL pdata\n", __func__);
		return -ENODEV;
	}

	/* set micbias voltage */
	vout_ctl = rouleur_get_micb_vout_ctl_val(pdata->micbias.micb1_mv);
	if (vout_ctl < 0) {
		rc = -EINVAL;
		goto done;
	}
	regmap_update_bits(rouleur->regmap, ROULEUR_ANA_MICBIAS_LDO_1_SETTING,
			   0xF8, vout_ctl << 3);
done:
	return rc;
}

static int rouleur_battery_supply_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct rouleur_priv *rouleur =
		container_of(nb, struct rouleur_priv, psy_nb);

	if (strcmp(psy->desc->name, "battery"))
		return NOTIFY_OK;
	queue_work(system_freezable_wq, &rouleur->soc_eval_work);

	return NOTIFY_OK;
}

static int rouleur_read_battery_soc(struct rouleur_priv *rouleur, int *soc_val)
{
	static struct power_supply *batt_psy;
	union power_supply_propval ret = {0,};
	int err = 0;

	*soc_val = 100;
	if (!batt_psy)
		batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		err = power_supply_get_property(batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		if (err) {
			pr_err("%s: battery SoC read error:%d\n",
				__func__, err);
			return err;
		}
		*soc_val = ret.intval;
	}
	pr_debug("%s: soc:%d\n", __func__, *soc_val);

	return err;
}

static void rouleur_evaluate_soc(struct work_struct *work)
{
	struct rouleur_priv *rouleur =
		container_of(work, struct rouleur_priv, soc_eval_work);
	int soc_val = 0, ret = 0;
	struct rouleur_pdata *pdata = NULL;

	pdata = dev_get_platdata(rouleur->dev);
	if (!pdata) {
		dev_err(rouleur->dev, "%s: pdata is NULL\n", __func__);
		return;
	}

	if (rouleur_read_battery_soc(rouleur, &soc_val) < 0) {
		dev_err(rouleur->dev, "%s unable to read battery SoC\n",
			__func__);
		return;
	}

	if (soc_val < SOC_THRESHOLD_LEVEL) {
		dev_dbg(rouleur->dev,
			"%s battery SoC less than threshold soc_val = %d\n",
			__func__, soc_val);
		/* Reduce PA Gain by 6DB for low SoC */
		if (rouleur->update_wcd_event)
			rouleur->update_wcd_event(rouleur->handle,
					SLV_BOLERO_EVT_RX_PA_GAIN_UPDATE,
					true);
		rouleur->low_soc = true;
		ret = msm_cdc_set_supply_min_voltage(rouleur->dev,
						 rouleur->supplies,
						 pdata->regulator,
						 pdata->num_supplies,
						 "cdc-vdd-mic-bias",
						 LOW_SOC_MBIAS_REG_MIN_VOLTAGE,
						 true);
		if (ret < 0)
			dev_err(rouleur->dev,
				"%s unable to set mbias min voltage\n",
				__func__);
	} else {
		if (rouleur->low_soc == true) {
			/* Reset PA Gain to default for normal SoC */
			if (rouleur->update_wcd_event)
				rouleur->update_wcd_event(rouleur->handle,
					SLV_BOLERO_EVT_RX_PA_GAIN_UPDATE,
					false);
			ret = msm_cdc_set_supply_min_voltage(rouleur->dev,
						rouleur->supplies,
						pdata->regulator,
						pdata->num_supplies,
						"cdc-vdd-mic-bias",
						LOW_SOC_MBIAS_REG_MIN_VOLTAGE,
						false);
			if (ret < 0)
				dev_err(rouleur->dev,
					"%s unable to set mbias min voltage\n",
					__func__);
			rouleur->low_soc = false;
		}
	}
}

static void rouleur_get_foundry_id(struct rouleur_priv *rouleur)
{
	int ret;

	if (rouleur->foundry_id_reg == 0) {
		pr_debug("%s: foundry id not defined\n", __func__);
		return;
	}

	ret = pm2250_spmi_read(rouleur->spmi_dev,
				rouleur->foundry_id_reg, &rouleur->foundry_id);
	if (ret == 0)
		pr_debug("%s: rouleur foundry id = %x\n", rouleur->foundry_id,
			 __func__);
	else
		pr_debug("%s: rouleur error in spmi read ret = %d\n",
			 __func__, ret);
}

static int rouleur_soc_codec_probe(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);
	int ret = -EINVAL;

	dev_info(component->dev, "%s()\n", __func__);
	rouleur = snd_soc_component_get_drvdata(component);

	if (!rouleur)
		return -EINVAL;

	rouleur->component = component;
	snd_soc_component_init_regmap(component, rouleur->regmap);

	rouleur->fw_data = devm_kzalloc(component->dev,
					sizeof(*(rouleur->fw_data)),
					GFP_KERNEL);
	if (!rouleur->fw_data) {
		dev_err(component->dev, "Failed to allocate fw_data\n");
		ret = -ENOMEM;
		goto done;
	}

	set_bit(WCD9XXX_MBHC_CAL, rouleur->fw_data->cal_bit);
	ret = wcd_cal_create_hwdep(rouleur->fw_data,
				   WCD9XXX_CODEC_HWDEP_NODE, component);

	if (ret < 0) {
		dev_err(component->dev, "%s hwdep failed %d\n", __func__, ret);
		goto done;
	}

	ret = rouleur_mbhc_init(&rouleur->mbhc, component, rouleur->fw_data);
	if (ret) {
		pr_err("%s: mbhc initialization failed\n", __func__);
		goto done;
	}
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "IN1_HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "IN2_HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "ADC1_OUTPUT");
	snd_soc_dapm_ignore_suspend(dapm, "ADC2_OUTPUT");
	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "LO");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1_OUTPUT");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2_OUTPUT");
	snd_soc_dapm_sync(dapm);

	rouleur_init_reg(component);
	/* Get rouleur foundry id */
	rouleur_get_foundry_id(rouleur);

	rouleur->version = ROULEUR_VERSION_1_0;
       /* Register event notifier */
	rouleur->nblock.notifier_call = rouleur_event_notify;
	if (rouleur->register_notifier) {
		ret = rouleur->register_notifier(rouleur->handle,
						&rouleur->nblock,
						true);
		if (ret) {
			dev_err(component->dev,
				"%s: Failed to register notifier %d\n",
				__func__, ret);
			return ret;
		}
	}
	rouleur->low_soc = false;
	rouleur->dev_up = true;
	/* Register notifier to change gain based on state of charge */
	INIT_WORK(&rouleur->soc_eval_work, rouleur_evaluate_soc);
	rouleur->psy_nb.notifier_call = rouleur_battery_supply_cb;
	if (power_supply_reg_notifier(&rouleur->psy_nb) < 0)
		dev_dbg(rouleur->dev,
			"%s: could not register pwr supply notifier\n",
			__func__);
	queue_work(system_freezable_wq, &rouleur->soc_eval_work);
done:
	return ret;
}

static void rouleur_soc_codec_remove(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	if (!rouleur)
		return;

	if (rouleur->register_notifier)
		rouleur->register_notifier(rouleur->handle,
						&rouleur->nblock,
						false);
}

static int rouleur_soc_codec_suspend(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	if (!rouleur)
		return 0;
	rouleur->dapm_bias_off = true;
	return 0;
}

static int rouleur_soc_codec_resume(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur = snd_soc_component_get_drvdata(component);

	if (!rouleur)
		return 0;
	rouleur->dapm_bias_off = false;
	return 0;
}

static const struct snd_soc_component_driver soc_codec_dev_rouleur = {
	.name = ROULEUR_DRV_NAME,
	.probe = rouleur_soc_codec_probe,
	.remove = rouleur_soc_codec_remove,
	.controls = rouleur_snd_controls,
	.num_controls = ARRAY_SIZE(rouleur_snd_controls),
	.dapm_widgets = rouleur_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rouleur_dapm_widgets),
	.dapm_routes = rouleur_audio_map,
	.num_dapm_routes = ARRAY_SIZE(rouleur_audio_map),
	.suspend = rouleur_soc_codec_suspend,
	.resume = rouleur_soc_codec_resume,
};

#ifdef CONFIG_PM_SLEEP
static int rouleur_suspend(struct device *dev)
{
	struct rouleur_priv *rouleur = NULL;
	int ret = 0;
	struct rouleur_pdata *pdata = NULL;

	if (!dev)
		return -ENODEV;

	rouleur = dev_get_drvdata(dev);
	if (!rouleur)
		return -EINVAL;

	pdata = dev_get_platdata(rouleur->dev);

	if (!pdata) {
		dev_err(dev, "%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	if (test_bit(ALLOW_VPOS_DISABLE, &rouleur->status_mask)) {
		ret = msm_cdc_disable_ondemand_supply(rouleur->dev,
						rouleur->supplies,
						pdata->regulator,
						pdata->num_supplies,
						"cdc-pa-vpos");
		if (ret == -EINVAL) {
			dev_err(dev, "%s: pa vpos is not disabled\n",
				__func__);
			return 0;
		}
		clear_bit(ALLOW_VPOS_DISABLE, &rouleur->status_mask);
	}
	if (rouleur->dapm_bias_off) {
		 msm_cdc_set_supplies_lpm_mode(rouleur->dev,
					      rouleur->supplies,
					      pdata->regulator,
					      pdata->num_supplies,
					      true);
		set_bit(WCD_SUPPLIES_LPM_MODE, &rouleur->status_mask);
	}
	return 0;
}

static int rouleur_resume(struct device *dev)
{
	struct rouleur_priv *rouleur = NULL;
	struct rouleur_pdata *pdata = NULL;

	if (!dev)
		return -ENODEV;

	rouleur = dev_get_drvdata(dev);
	if (!rouleur)
		return -EINVAL;

	pdata = dev_get_platdata(rouleur->dev);

	if (!pdata) {
		dev_err(dev, "%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	if (test_bit(WCD_SUPPLIES_LPM_MODE, &rouleur->status_mask)) {
		msm_cdc_set_supplies_lpm_mode(rouleur->dev,
						rouleur->supplies,
						pdata->regulator,
						pdata->num_supplies,
						false);
		clear_bit(WCD_SUPPLIES_LPM_MODE, &rouleur->status_mask);
	}

	return 0;
}
#endif

static int rouleur_reset(struct device *dev, int reset_val)
{
	struct rouleur_priv *rouleur = NULL;

	if (!dev)
		return -ENODEV;

	rouleur = dev_get_drvdata(dev);
	if (!rouleur)
		return -EINVAL;

	pm2250_spmi_write(rouleur->spmi_dev, rouleur->reset_reg, reset_val);

	return 0;
}

static int rouleur_read_of_property_u32(struct device *dev, const char *name,
					u32 *val)
{
	int rc = 0;

	rc = of_property_read_u32(dev->of_node, name, val);
	if (rc)
		dev_err(dev, "%s: Looking up %s property in node %s failed\n",
			__func__, name, dev->of_node->full_name);

	return rc;
}

static void rouleur_dt_parse_micbias_info(struct device *dev,
					  struct rouleur_micbias_setting *mb)
{
	u32 prop_val = 0;
	int rc = 0;

	/* MB1 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias1-mv",
				    NULL)) {
		rc = rouleur_read_of_property_u32(dev,
						  "qcom,cdc-micbias1-mv",
						  &prop_val);
		if (!rc)
			mb->micb1_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias1 DT property not found\n",
			__func__);
	}

	/* MB2 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias2-mv",
				    NULL)) {
		rc = rouleur_read_of_property_u32(dev,
						  "qcom,cdc-micbias2-mv",
						  &prop_val);
		if (!rc)
			mb->micb2_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias2 DT property not found\n",
			__func__);
	}

	/* MB3 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias3-mv",
				    NULL)) {
		rc = rouleur_read_of_property_u32(dev,
						  "qcom,cdc-micbias3-mv",
						  &prop_val);
		if (!rc)
			mb->micb3_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias3 DT property not found\n",
			__func__);
	}
}

struct rouleur_pdata *rouleur_populate_dt_data(struct device *dev)
{
	struct rouleur_pdata *pdata = NULL;
	u32 reg;
	int ret = 0;

	pdata = kzalloc(sizeof(struct rouleur_pdata),
				GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->spmi_np = of_parse_phandle(dev->of_node,
					"qcom,pmic-spmi-node", 0);
	if (!pdata->spmi_np) {
		dev_err(dev, "%s: Looking up %s property in node %s failed\n",
				__func__, "qcom,pmic-spmi-node",
				dev->of_node->full_name);
		kfree(pdata);
		return NULL;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,wcd-reset-reg", &reg);
	if (ret) {
		dev_err(dev, "%s: Failed to obtain reset reg value %d\n",
			__func__, ret);
		kfree(pdata);
		return NULL;
	}
	pdata->reset_reg = reg;

	if (of_property_read_u32(dev->of_node, "qcom,foundry-id-reg", &reg))
		dev_dbg(dev, "%s: Failed to obtain foundry id\n",
			__func__);
	else
		pdata->foundry_id_reg = reg;

	/* Parse power supplies */
	msm_cdc_get_power_supplies(dev, &pdata->regulator,
				   &pdata->num_supplies);
	if (!pdata->regulator || (pdata->num_supplies <= 0)) {
		dev_err(dev, "%s: no power supplies defined for codec\n",
			__func__);
		kfree(pdata);
		return NULL;
	}

	pdata->rx_slave = of_parse_phandle(dev->of_node, "qcom,rx-slave", 0);
	pdata->tx_slave = of_parse_phandle(dev->of_node, "qcom,tx-slave", 0);
	rouleur_dt_parse_micbias_info(dev, &pdata->micbias);

	return pdata;
}

static int rouleur_wakeup(void *handle, bool enable)
{
	struct rouleur_priv *priv;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	priv = (struct rouleur_priv *)handle;
	if (!priv->tx_swr_dev) {
		pr_err("%s: tx swr dev is NULL\n", __func__);
		return -EINVAL;
	}
	if (enable)
		return swr_device_wakeup_vote(priv->tx_swr_dev);
	else
		return swr_device_wakeup_unvote(priv->tx_swr_dev);
}

static irqreturn_t rouleur_wd_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: Watchdog interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static int rouleur_bind(struct device *dev)
{
	int ret = 0, i = 0;
	struct rouleur_priv *rouleur = NULL;
	struct rouleur_pdata *pdata = NULL;
	struct wcd_ctrl_platform_data *plat_data = NULL;
	struct platform_device *pdev = NULL;

	rouleur = kzalloc(sizeof(struct rouleur_priv), GFP_KERNEL);
	if (!rouleur)
		return -ENOMEM;

	dev_set_drvdata(dev, rouleur);

	pdata = rouleur_populate_dt_data(dev);
	if (!pdata) {
		dev_err(dev, "%s: Fail to obtain platform data\n", __func__);
		kfree(rouleur);
		return -EINVAL;
	}
	rouleur->dev = dev;
	rouleur->dev->platform_data = pdata;
	pdev = of_find_device_by_node(pdata->spmi_np);
	if (!pdev) {
		dev_err(dev, "%s: platform device from SPMI node is NULL\n",
				__func__);
		ret = -EINVAL;
		goto err_bind_all;
	}

	rouleur->spmi_dev = &pdev->dev;
	rouleur->reset_reg = pdata->reset_reg;
	rouleur->foundry_id_reg = pdata->foundry_id_reg;
	ret = msm_cdc_init_supplies(dev, &rouleur->supplies,
				    pdata->regulator, pdata->num_supplies);
	if (!rouleur->supplies) {
		dev_err(dev, "%s: Cannot init wcd supplies\n",
			__func__);
		goto err_bind_all;
	}

	plat_data = dev_get_platdata(dev->parent);
	if (!plat_data) {
		dev_err(dev, "%s: platform data from parent is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_bind_all;
	}
	rouleur->handle = (void *)plat_data->handle;
	if (!rouleur->handle) {
		dev_err(dev, "%s: handle is NULL\n", __func__);
		ret = -EINVAL;
		goto err_bind_all;
	}
	rouleur->update_wcd_event = plat_data->update_wcd_event;
	if (!rouleur->update_wcd_event) {
		dev_err(dev, "%s: update_wcd_event api is null!\n",
			__func__);
		ret = -EINVAL;
		goto err_bind_all;
	}
	rouleur->register_notifier = plat_data->register_notifier;
	if (!rouleur->register_notifier) {
		dev_err(dev, "%s: register_notifier api is null!\n",
			__func__);
		ret = -EINVAL;
		goto err_bind_all;
	}

	ret = msm_cdc_enable_static_supplies(dev, rouleur->supplies,
					     pdata->regulator,
					     pdata->num_supplies);
	if (ret) {
		dev_err(dev, "%s: wcd static supply enable failed!\n",
			__func__);
		goto err_bind_all;
	}

	rouleur_reset(dev, 0x01);
	usleep_range(20, 30);
	rouleur_reset(dev, 0x00);
	/*
	 * Add 5msec delay to provide sufficient time for
	 * soundwire auto enumeration of slave devices as
	 * as per HW requirement.
	 */
	usleep_range(5000, 5010);
	rouleur->wakeup = rouleur_wakeup;

	ret = component_bind_all(dev, rouleur);
	if (ret) {
		dev_err(dev, "%s: Slave bind failed, ret = %d\n",
			__func__, ret);
		goto err_bind_all;
	}

	ret = rouleur_parse_port_mapping(dev, "qcom,rx_swr_ch_map", CODEC_RX);
	ret |= rouleur_parse_port_mapping(dev, "qcom,tx_swr_ch_map", CODEC_TX);

	if (ret) {
		dev_err(dev, "Failed to read port mapping\n");
		goto err;
	}

	rouleur->rx_swr_dev = get_matching_swr_slave_device(pdata->rx_slave);
	if (!rouleur->rx_swr_dev) {
		dev_err(dev, "%s: Could not find RX swr slave device\n",
			 __func__);
		ret = -ENODEV;
		goto err;
	}

	rouleur->tx_swr_dev = get_matching_swr_slave_device(pdata->tx_slave);
	if (!rouleur->tx_swr_dev) {
		dev_err(dev, "%s: Could not find TX swr slave device\n",
			__func__);
		ret = -ENODEV;
		goto err;
	}

	rouleur->regmap = devm_regmap_init_swr(rouleur->tx_swr_dev,
					       &rouleur_regmap_config);
	if (!rouleur->regmap) {
		dev_err(dev, "%s: Regmap init failed\n",
				__func__);
		goto err;
	}

	/* Set all interupts as edge triggered */
	for (i = 0; i < rouleur_regmap_irq_chip.num_regs; i++)
		regmap_write(rouleur->regmap,
			     (ROULEUR_DIG_SWR_INTR_LEVEL_0 + i), 0);

	rouleur_regmap_irq_chip.irq_drv_data = rouleur;
	rouleur->irq_info.wcd_regmap_irq_chip = &rouleur_regmap_irq_chip;
	rouleur->irq_info.codec_name = "rouleur";
	rouleur->irq_info.regmap = rouleur->regmap;
	rouleur->irq_info.dev = dev;
	ret = wcd_irq_init(&rouleur->irq_info, &rouleur->virq);

	if (ret) {
		dev_err(dev, "%s: IRQ init failed: %d\n",
			__func__, ret);
		goto err;
	}
	rouleur->tx_swr_dev->slave_irq = rouleur->virq;

	mutex_init(&rouleur->micb_lock);
	mutex_init(&rouleur->main_bias_lock);
	mutex_init(&rouleur->rx_clk_lock);

	ret = rouleur_set_micbias_data(rouleur, pdata);
	if (ret < 0) {
		dev_err(dev, "%s: bad micbias pdata\n", __func__);
		goto err_irq;
	}

	/* Request for watchdog interrupt */
	wcd_request_irq(&rouleur->irq_info, ROULEUR_IRQ_HPHR_PDM_WD_INT,
			"HPHR PDM WD INT", rouleur_wd_handle_irq, NULL);
	wcd_request_irq(&rouleur->irq_info, ROULEUR_IRQ_HPHL_PDM_WD_INT,
			"HPHL PDM WD INT", rouleur_wd_handle_irq, NULL);
	/* Disable watchdog interrupt for HPH */
	wcd_disable_irq(&rouleur->irq_info, ROULEUR_IRQ_HPHR_PDM_WD_INT);
	wcd_disable_irq(&rouleur->irq_info, ROULEUR_IRQ_HPHL_PDM_WD_INT);

	ret = snd_soc_register_component(dev, &soc_codec_dev_rouleur,
				     rouleur_dai, ARRAY_SIZE(rouleur_dai));
	if (ret) {
		dev_err(dev, "%s: Codec registration failed\n",
				__func__);
		goto err_irq;
	}

	return ret;
err_irq:
	wcd_irq_exit(&rouleur->irq_info, rouleur->virq);
	mutex_destroy(&rouleur->micb_lock);
	mutex_destroy(&rouleur->main_bias_lock);
	mutex_destroy(&rouleur->rx_clk_lock);
err:
	component_unbind_all(dev, rouleur);
err_bind_all:
	dev_set_drvdata(dev, NULL);
	kfree(pdata);
	kfree(rouleur);
	return ret;
}

static void rouleur_unbind(struct device *dev)
{
	struct rouleur_priv *rouleur = dev_get_drvdata(dev);
	struct rouleur_pdata *pdata = dev_get_platdata(rouleur->dev);

	wcd_irq_exit(&rouleur->irq_info, rouleur->virq);
	snd_soc_unregister_component(dev);
	component_unbind_all(dev, rouleur);
	mutex_destroy(&rouleur->micb_lock);
	mutex_destroy(&rouleur->main_bias_lock);
	mutex_destroy(&rouleur->rx_clk_lock);
	dev_set_drvdata(dev, NULL);
	kfree(pdata);
	kfree(rouleur);
}

static const struct of_device_id rouleur_dt_match[] = {
	{ .compatible = "qcom,rouleur-codec" , .data = "rouleur" },
	{}
};

static const struct component_master_ops rouleur_comp_ops = {
	.bind   = rouleur_bind,
	.unbind = rouleur_unbind,
};

static int rouleur_compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static void rouleur_release_of(struct device *dev, void *data)
{
	of_node_put(data);
}

static int rouleur_add_slave_components(struct device *dev,
				struct component_match **matchptr)
{
	struct device_node *np, *rx_node, *tx_node;

	np = dev->of_node;

	rx_node = of_parse_phandle(np, "qcom,rx-slave", 0);
	if (!rx_node) {
		dev_err(dev, "%s: Rx-slave node not defined\n", __func__);
		return -ENODEV;
	}
	of_node_get(rx_node);
	component_match_add_release(dev, matchptr,
			rouleur_release_of,
			rouleur_compare_of,
			rx_node);

	tx_node = of_parse_phandle(np, "qcom,tx-slave", 0);
	if (!tx_node) {
		dev_err(dev, "%s: Tx-slave node not defined\n", __func__);
			return -ENODEV;
	}
	of_node_get(tx_node);
	component_match_add_release(dev, matchptr,
			rouleur_release_of,
			rouleur_compare_of,
			tx_node);
	return 0;
}

static int rouleur_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;
	int ret;

	ret = rouleur_add_slave_components(&pdev->dev, &match);
	if (ret)
		return ret;

	return component_master_add_with_match(&pdev->dev,
					&rouleur_comp_ops, match);
}

static int rouleur_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &rouleur_comp_ops);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops rouleur_dev_pm_ops = {
	.suspend_late = rouleur_suspend,
	.resume_early = rouleur_resume
};
#endif

static struct platform_driver rouleur_codec_driver = {
	.probe = rouleur_probe,
	.remove = rouleur_remove,
	.driver = {
		.name = "rouleur_codec",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rouleur_dt_match),
#ifdef CONFIG_PM_SLEEP
		.pm = &rouleur_dev_pm_ops,
#endif
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(rouleur_codec_driver);
MODULE_DESCRIPTION("Rouleur Codec driver");
MODULE_LICENSE("GPL v2");

