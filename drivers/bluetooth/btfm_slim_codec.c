// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/slimbus/slimbus.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "btfm_slim.h"

static int bt_soc_enable_status;
int btfm_feedback_ch_setting;

static int btfm_slim_codec_write(struct snd_soc_component *codec,
			unsigned int reg, unsigned int value)
{
	return 0;
}

static unsigned int btfm_slim_codec_read(struct snd_soc_component *codec,
				unsigned int reg)
{
	return 0;
}

static int bt_soc_status_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = bt_soc_enable_status;
	return 1;
}

static int bt_soc_status_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 1;
}

static int btfm_get_feedback_ch_setting(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = btfm_feedback_ch_setting;
	return 1;
}

static int btfm_put_feedback_ch_setting(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	btfm_feedback_ch_setting = ucontrol->value.integer.value[0];
	return 1;
}

static const struct snd_kcontrol_new status_controls[] = {
	SOC_SINGLE_EXT("BT SOC status", 0, 0, 1, 0,
			bt_soc_status_get,
			bt_soc_status_put),
	SOC_SINGLE_EXT("BT set feedback channel", 0, 0, 1, 0,
	btfm_get_feedback_ch_setting,
	btfm_put_feedback_ch_setting)
};


static int btfm_slim_codec_probe(struct snd_soc_component *codec)
{
	snd_soc_add_component_controls(codec, status_controls,
				   ARRAY_SIZE(status_controls));
	return 0;
}

static void btfm_slim_codec_remove(struct snd_soc_component *codec)
{

}

static int btfm_slim_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret;
	struct btfmslim *btfmslim = dai->dev->platform_data;

	BTFMSLIM_DBG("substream = %s  stream = %d dai->name = %s",
		 substream->name, substream->stream, dai->name);
	ret = btfm_slim_hw_init(btfmslim);
	return ret;
}

static void btfm_slim_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int i;
	struct btfmslim *btfmslim = dai->dev->platform_data;
	struct btfmslim_ch *ch;
	uint8_t rxport, grp = false, nchan = 1;

	BTFMSLIM_DBG("dai->name: %s, dai->id: %d, dai->rate: %d", dai->name,
		dai->id, dai->rate);

	switch (dai->id) {
	case BTFM_FM_SLIM_TX:
		grp = true; nchan = 2;
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_SLIM_TX:
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_A2DP_SLIM_RX:
	case BTFM_BT_SPLIT_A2DP_SLIM_RX:
		ch = btfmslim->rx_chs;
		rxport = 1;
		break;
	case BTFM_SLIM_NUM_CODEC_DAIS:
	default:
		BTFMSLIM_ERR("dai->id is invalid:%d", dai->id);
		return;
	}

	/* Search for dai->id matched port handler */
	for (i = 0; (i < BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != dai->id); ch++, i++)
		;

	if ((ch->port == BTFM_SLIM_PGD_PORT_LAST) ||
		(ch->id == BTFM_SLIM_NUM_CODEC_DAIS)) {
		BTFMSLIM_ERR("ch is invalid!!");
		return;
	}

	btfm_slim_disable_ch(btfmslim, ch, rxport, grp, nchan);
	btfm_slim_hw_deinit(btfmslim);
}

static int btfm_slim_dai_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	BTFMSLIM_DBG("dai->name = %s DAI-ID %x rate %d num_ch %d",
		dai->name, dai->id, params_rate(params),
		params_channels(params));

	return 0;
}

static int btfm_slim_dai_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int i, ret = -EINVAL;
	struct btfmslim *btfmslim = dai->dev->platform_data;
	struct btfmslim_ch *ch;
	uint8_t rxport, grp = false, nchan = 1;

	bt_soc_enable_status = 0;

	BTFMSLIM_DBG("dai->name: %s, dai->id: %d, dai->rate: %d", dai->name,
		dai->id, dai->rate);

	/* save sample rate */
	btfmslim->sample_rate = dai->rate;

	switch (dai->id) {
	case BTFM_FM_SLIM_TX:
		grp = true; nchan = 2;
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_SLIM_TX:
		ch = btfmslim->tx_chs;
		rxport = 0;
		break;
	case BTFM_BT_SCO_A2DP_SLIM_RX:
	case BTFM_BT_SPLIT_A2DP_SLIM_RX:
		ch = btfmslim->rx_chs;
		rxport = 1;
		break;
	case BTFM_SLIM_NUM_CODEC_DAIS:
	default:
		BTFMSLIM_ERR("dai->id is invalid:%d", dai->id);
		return ret;
	}

	/* Search for dai->id matched port handler */
	for (i = 0; (i < BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != BTFM_SLIM_NUM_CODEC_DAIS) &&
		(ch->id != dai->id); ch++, i++)
		;

	if ((ch->port == BTFM_SLIM_PGD_PORT_LAST) ||
		(ch->id == BTFM_SLIM_NUM_CODEC_DAIS)) {
		BTFMSLIM_ERR("ch is invalid!!");
		return ret;
	}

	ret = btfm_slim_enable_ch(btfmslim, ch, rxport, dai->rate, grp, nchan);

	/* save the enable channel status */
	if (ret == 0)
		bt_soc_enable_status = 1;

	if (ret == -EISCONN) {
		BTFMSLIM_ERR("channel opened without closing, return success");
		ret = 0;
	}
	return ret;
}

/* This function will be called once during boot up */
static int btfm_slim_dai_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	int ret = -EINVAL, i;
	struct btfmslim *btfmslim = dai->dev->platform_data;
	struct btfmslim_ch *rx_chs;
	struct btfmslim_ch *tx_chs;

	BTFMSLIM_DBG("");

	if (!btfmslim)
		return ret;

	rx_chs = btfmslim->rx_chs;
	tx_chs = btfmslim->tx_chs;

	if (!rx_chs || !tx_chs)
		return ret;

	BTFMSLIM_DBG("Rx: id\tname\tport\thdl\tch\tch_hdl");
	for (i = 0; (rx_chs->port != BTFM_SLIM_PGD_PORT_LAST) && (i < rx_num);
		i++, rx_chs++) {
		/* Set Rx Channel number from machine driver and
		 * get channel handler from slimbus driver
		 */
		rx_chs->ch = *(uint8_t *)(rx_slot + i);
		ret = slim_query_ch(btfmslim->slim_pgd, rx_chs->ch,
			&rx_chs->ch_hdl);
		if (ret < 0) {
			BTFMSLIM_ERR("slim_query_ch failure ch#%d - ret[%d]",
				rx_chs->ch, ret);
			goto error;
		}
		BTFMSLIM_DBG("    %d\t%s\t%d\t%x\t%d\t%x", rx_chs->id,
			rx_chs->name, rx_chs->port, rx_chs->port_hdl,
			rx_chs->ch, rx_chs->ch_hdl);
	}

	BTFMSLIM_DBG("Tx: id\tname\tport\thdl\tch\tch_hdl");
	for (i = 0; (tx_chs->port != BTFM_SLIM_PGD_PORT_LAST) && (i < tx_num);
		i++, tx_chs++) {
		/* Set Tx Channel number from machine driver and
		 * get channel handler from slimbus driver
		 */
		tx_chs->ch = *(uint8_t *)(tx_slot + i);
		ret = slim_query_ch(btfmslim->slim_pgd, tx_chs->ch,
			&tx_chs->ch_hdl);
		if (ret < 0) {
			BTFMSLIM_ERR("slim_query_ch failure ch#%d - ret[%d]",
				tx_chs->ch, ret);
			goto error;
		}
		BTFMSLIM_DBG("    %d\t%s\t%d\t%x\t%d\t%x", tx_chs->id,
			tx_chs->name, tx_chs->port, tx_chs->port_hdl,
			tx_chs->ch, tx_chs->ch_hdl);
	}

error:
	return ret;
}

static int btfm_slim_dai_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)
{
	int i, ret = -EINVAL, *slot = NULL, j = 0, num = 1;
	struct btfmslim *btfmslim = dai->dev->platform_data;
	struct btfmslim_ch *ch = NULL;

	if (!btfmslim)
		return ret;

	switch (dai->id) {
	case BTFM_FM_SLIM_TX:
		num = 2;
	case BTFM_BT_SCO_SLIM_TX:
		if (!tx_slot || !tx_num) {
			BTFMSLIM_ERR("Invalid tx_slot %p or tx_num %p",
				tx_slot, tx_num);
			return -EINVAL;
		}
		ch = btfmslim->tx_chs;
		if (!ch)
			return -EINVAL;
		slot = tx_slot;
		*rx_slot = 0;
		*tx_num = num;
		*rx_num = 0;
		break;
	case BTFM_BT_SCO_A2DP_SLIM_RX:
	case BTFM_BT_SPLIT_A2DP_SLIM_RX:
		if (!rx_slot || !rx_num) {
			BTFMSLIM_ERR("Invalid rx_slot %p or rx_num %p",
				 rx_slot, rx_num);
			return -EINVAL;
		}
		ch = btfmslim->rx_chs;
		if (!ch)
			return -EINVAL;
		slot = rx_slot;
		*tx_slot = 0;
		*tx_num = 0;
		*rx_num = num;
		break;
	default:
		BTFMSLIM_ERR("Unsupported DAI %d", dai->id);
		return -EINVAL;
	}

	do {
		if (!ch)
			return -EINVAL;
		for (i = 0; (i < BTFM_SLIM_NUM_CODEC_DAIS) && (ch->id !=
			BTFM_SLIM_NUM_CODEC_DAIS) && (ch->id != dai->id);
			ch++, i++)
			;

		if (ch->id == BTFM_SLIM_NUM_CODEC_DAIS ||
			i == BTFM_SLIM_NUM_CODEC_DAIS) {
			BTFMSLIM_ERR(
				"No channel has been allocated for dai (%d)",
				dai->id);
			return -EINVAL;
		}
		if (!slot)
			return -EINVAL;
		*(slot + j) = ch->ch;
		BTFMSLIM_DBG("id:%d, port:%d, ch:%d, slot: %d", ch->id,
			ch->port, ch->ch, *(slot + j));

		/* In case it has mulitiple channels */
		if (++j < num)
			ch++;
	} while (j < num);

	return 0;
}

static struct snd_soc_dai_ops btfmslim_dai_ops = {
	.startup = btfm_slim_dai_startup,
	.shutdown = btfm_slim_dai_shutdown,
	.hw_params = btfm_slim_dai_hw_params,
	.prepare = btfm_slim_dai_prepare,
	.set_channel_map = btfm_slim_dai_set_channel_map,
	.get_channel_map = btfm_slim_dai_get_channel_map,
};

static struct snd_soc_dai_driver btfmslim_dai[] = {
	{	/* FM Audio data multiple channel  : FM -> qdsp */
		.name = "btfm_fm_slim_tx",
		.id = BTFM_FM_SLIM_TX,
		.capture = {
			.stream_name = "FM TX Capture",
			.rates = SNDRV_PCM_RATE_48000, /* 48 KHz */
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &btfmslim_dai_ops,
	},
	{	/* Bluetooth SCO voice uplink: bt -> modem */
		.name = "btfm_bt_sco_slim_tx",
		.id = BTFM_BT_SCO_SLIM_TX,
		.capture = {
			.stream_name = "SCO TX Capture",
			/* 8 KHz or 16 KHz */
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
				| SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 96000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &btfmslim_dai_ops,
	},
	{	/* Bluetooth SCO voice downlink: modem -> bt or A2DP Playback */
		.name = "btfm_bt_sco_a2dp_slim_rx",
		.id = BTFM_BT_SCO_A2DP_SLIM_RX,
		.playback = {
			.stream_name = "SCO A2DP RX Playback",
			/* 8/16/44.1/48/88.2/96 Khz */
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
				| SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 96000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &btfmslim_dai_ops,
	},
	{	/* Bluetooth Split A2DP data: qdsp -> bt */
		.name = "btfm_bt_split_a2dp_slim_rx",
		.id = BTFM_BT_SPLIT_A2DP_SLIM_RX,
		.playback = {
			.stream_name = "SPLIT A2DP Playback",
			.rates = SNDRV_PCM_RATE_48000, /* 48 KHz */
			.formats = SNDRV_PCM_FMTBIT_S16_LE, /* 16 bits */
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &btfmslim_dai_ops,
	},
};

static const struct snd_soc_component_driver btfmslim_codec = {
	.probe	= btfm_slim_codec_probe,
	.remove	= btfm_slim_codec_remove,
	.read	= btfm_slim_codec_read,
	.write	= btfm_slim_codec_write,
};

int btfm_slim_register_codec(struct device *dev)
{
	int ret = 0;

	BTFMSLIM_DBG("");
	/* Register Codec driver */
	ret = snd_soc_register_component(dev, &btfmslim_codec,
		btfmslim_dai, ARRAY_SIZE(btfmslim_dai));

	if (ret)
		BTFMSLIM_ERR("failed to register codec (%d)", ret);

	return ret;
}

void btfm_slim_unregister_codec(struct device *dev)
{
	BTFMSLIM_DBG("");
	/* Unregister Codec driver */
	snd_soc_unregister_component(dev);
}

MODULE_DESCRIPTION("BTFM Slimbus Codec driver");
MODULE_LICENSE("GPL v2");
