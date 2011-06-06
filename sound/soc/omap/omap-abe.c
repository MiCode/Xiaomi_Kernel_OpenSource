/*
 * omap-abe.c  --  OMAP ALSA SoC DAI driver using Audio Backend
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@ti.com>
 *          Misael Lopez Cruz <misael.lopez@ti.com>
 *          Sebastien Guiriec <s-guiriec@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dsp.h>

#include <plat/dma-44xx.h>
#include <plat/dma.h>
#include "omap-pcm.h"
#include "omap-abe.h"
#include "omap-abe-dsp.h"
#include "abe/abe_main.h"
#include "abe/port_mgr.h"

#define OMAP_ABE_FORMATS	SNDRV_PCM_FMTBIT_S32_LE

struct omap_abe_data {
	/* MODEM FE*/
	struct snd_pcm_substream *modem_substream[2];
	struct snd_soc_dai *modem_dai;

	struct abe *abe;

	/* BE & FE Ports */
	struct omap_abe_port *port[OMAP_ABE_MAX_PORT_ID + 1];
};

/*
 * Stream DMA parameters
 */
static struct omap_pcm_dma_data omap_abe_dai_dma_params[7][2] = {
{
	{
		.name = "Media Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_0,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
	{
		.name = "Media Capture1",
		.dma_req = OMAP44XX_DMA_ABE_REQ_3,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
},
{
	{},
	{
		.name = "Media Capture2",
		.dma_req = OMAP44XX_DMA_ABE_REQ_4,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
},
{
	{
		.name = "Voice Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_1,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
	{
		.name = "Voice Capture",
		.dma_req = OMAP44XX_DMA_ABE_REQ_2,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
},
{
	{
		.name = "Tones Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_5,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},{},
},
{
	{
		.name = "Vibra Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_6,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},{},
},
{
	{
		.name = "MODEM Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_1,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
	{
		.name = "MODEM Capture",
		.dma_req = OMAP44XX_DMA_ABE_REQ_2,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},
},
{
	{
		.name = "Low Power Playback",
		.dma_req = OMAP44XX_DMA_ABE_REQ_0,
		.data_type = OMAP_DMA_DATA_TYPE_S32,
		.sync_mode = OMAP_DMA_SYNC_PACKET,
	},{},
},};

static int modem_get_dai(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *modem_rtd;

	abe_priv->modem_substream[substream->stream] =
			snd_soc_get_dai_substream(rtd->card,
					OMAP_ABE_BE_MM_EXT1, substream->stream);

	if (abe_priv->modem_substream[substream->stream] == NULL)
		return -ENODEV;

	modem_rtd = abe_priv->modem_substream[substream->stream]->private_data;
	abe_priv->modem_substream[substream->stream]->runtime = substream->runtime;
	abe_priv->modem_dai = modem_rtd->cpu_dai;

	return 0;
}

static void mute_be(struct snd_soc_pcm_runtime *be,
		struct snd_soc_dai *dai, int stream)
{
	dev_dbg(&be->dev, "%s: %s %d\n", __func__, be->cpu_dai->name, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (be->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
			abe_write_gain(GAINS_DL1, MUTE_GAIN, RAMP_5MS,
				GAIN_LEFT_OFFSET);
			abe_write_gain(GAINS_DL1, MUTE_GAIN, RAMP_5MS,
				GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			abe_write_gain(GAINS_DL2, MUTE_GAIN, RAMP_5MS,
				GAIN_LEFT_OFFSET);
			abe_write_gain(GAINS_DL2, MUTE_GAIN, RAMP_5MS,
				GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_PDM_VIB:
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		}
	} else {
		switch (be->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_UL:
			break;
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
		case OMAP_ABE_DAI_DMIC0:
		case OMAP_ABE_DAI_DMIC1:
		case OMAP_ABE_DAI_DMIC2:
			break;
		}
	}
}

static void unmute_be(struct snd_soc_pcm_runtime *be,
		struct snd_soc_dai *dai, int stream)
{
	dev_dbg(&be->dev, "%s: %s %d\n", __func__, be->cpu_dai->name, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (be->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_DL1:
			abe_write_gain(GAINS_DL1, GAIN_0dB, RAMP_5MS,
				GAIN_LEFT_OFFSET);
			abe_write_gain(GAINS_DL1, GAIN_0dB, RAMP_5MS,
				GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_PDM_DL2:
			abe_write_gain(GAINS_DL2, GAIN_0dB, RAMP_5MS,
				GAIN_LEFT_OFFSET);
			abe_write_gain(GAINS_DL2, GAIN_0dB, RAMP_5MS,
				GAIN_RIGHT_OFFSET);
			break;
		case OMAP_ABE_DAI_PDM_VIB:
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
			break;
		}
	} else {

		switch (be->dai_link->be_id) {
		case OMAP_ABE_DAI_PDM_UL:
			break;
		case OMAP_ABE_DAI_BT_VX:
		case OMAP_ABE_DAI_MM_FM:
		case OMAP_ABE_DAI_MODEM:
		case OMAP_ABE_DAI_DMIC0:
		case OMAP_ABE_DAI_DMIC1:
		case OMAP_ABE_DAI_DMIC2:
			break;
		}
	}
}

static void enable_be_port(struct snd_soc_pcm_runtime *be,
		struct snd_soc_dai *dai, int stream)
{
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);
	abe_data_format_t format;

	dev_dbg(&be->dev, "%s: %s %d\n", __func__, be->cpu_dai->name, stream);

	switch (be->dai_link->be_id) {
	/* McPDM Downlink is special case and handled by McPDM driver */
	case OMAP_ABE_DAI_PDM_DL1:
	case OMAP_ABE_DAI_PDM_DL2:
	case OMAP_ABE_DAI_PDM_VIB:
		break;
	case OMAP_ABE_DAI_PDM_UL:
		omap_abe_port_enable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_PDM_UL1]);
		break;
	case OMAP_ABE_DAI_BT_VX:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe_priv->abe,
					abe_priv->port[OMAP_ABE_BE_PORT_BT_VX_DL]))
				return;

			/* BT_DL connection to McBSP 1 ports */
			format.f = 8000;
			format.samp_format = MONO_RSHIFTED_16;
			abe_connect_serial_port(BT_VX_DL_PORT, &format, MCBSP1_TX);
			omap_abe_port_enable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_BT_VX_DL]);
		} else {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe_priv->abe,
					abe_priv->port[OMAP_ABE_BE_PORT_BT_VX_UL]))
				return;

			/* BT_UL connection to McBSP 1 ports */
			format.f = 8000;
			format.samp_format = MONO_RSHIFTED_16;
			abe_connect_serial_port(BT_VX_UL_PORT, &format, MCBSP1_RX);
			omap_abe_port_enable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_BT_VX_UL]);
		}
		break;
	case OMAP_ABE_DAI_MM_FM:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe_priv->abe,
					abe_priv->port[OMAP_ABE_BE_PORT_MM_EXT_DL]))
				return;

			/* MM_EXT connection to McBSP 2 ports */
			format.f = 48000;
			format.samp_format = STEREO_RSHIFTED_16;
			abe_connect_serial_port(MM_EXT_OUT_PORT, &format, MCBSP2_TX);
			omap_abe_port_enable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_MM_EXT_DL]);
		} else {

			/* port can only be configured if it's not running */
			if (omap_abe_port_is_enabled(abe_priv->abe,
					abe_priv->port[OMAP_ABE_BE_PORT_MM_EXT_UL]))
				return;

			/* MM_EXT connection to McBSP 2 ports */
			format.f = 48000;
			format.samp_format = STEREO_RSHIFTED_16;
			abe_connect_serial_port(MM_EXT_IN_PORT, &format, MCBSP2_RX);
			omap_abe_port_enable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_MM_EXT_UL]);
		}
		break;
	case OMAP_ABE_DAI_DMIC0:
		omap_abe_port_enable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_DMIC0]);
		break;
	case OMAP_ABE_DAI_DMIC1:
		omap_abe_port_enable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_DMIC1]);
		break;
	case OMAP_ABE_DAI_DMIC2:
		omap_abe_port_enable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_DMIC2]);
		break;
	}
}

static void enable_fe_port(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);

	dev_dbg(&fe->dev, "%s: %s %d\n", __func__, dai->name, stream);

	switch(dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_enable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_MM_DL1]);
		else
			omap_abe_port_enable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_MM_UL1]);
		break;
	case ABE_FRONTEND_DAI_LP_MEDIA:
		abe_enable_data_transfer(MM_DL_PORT);
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			omap_abe_port_enable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_MM_UL2]);
		break;
	case ABE_FRONTEND_DAI_MODEM:
	case ABE_FRONTEND_DAI_VOICE:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_enable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_VX_DL]);
		else
			omap_abe_port_enable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_VX_UL]);
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_enable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_TONES]);
		break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_enable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_VIB]);
		break;
	}
}

static void disable_be_port(struct snd_soc_pcm_runtime *be,
		struct snd_soc_dai *dai, int stream)
{
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);

	dev_dbg(&be->dev, "%s: %s %d\n", __func__, be->cpu_dai->name, stream);

	switch (be->dai_link->be_id) {
	/* McPDM Downlink is special case and handled by McPDM driver */
	case OMAP_ABE_DAI_PDM_DL1:
	case OMAP_ABE_DAI_PDM_DL2:
	case OMAP_ABE_DAI_PDM_VIB:
		break;
	case OMAP_ABE_DAI_PDM_UL:
		omap_abe_port_disable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_PDM_UL1]);
		break;
	case OMAP_ABE_DAI_BT_VX:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_BE_PORT_BT_VX_DL]);
		else
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_BE_PORT_BT_VX_UL]);
		break;
	case OMAP_ABE_DAI_MM_FM:
	case OMAP_ABE_DAI_MODEM:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_BE_PORT_MM_EXT_DL]);
		else
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_BE_PORT_MM_EXT_UL]);
		break;
	case OMAP_ABE_DAI_DMIC0:
		omap_abe_port_disable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_DMIC0]);
		break;
	case OMAP_ABE_DAI_DMIC1:
		omap_abe_port_disable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_DMIC1]);
		break;
	case OMAP_ABE_DAI_DMIC2:
		omap_abe_port_disable(abe_priv->abe,
				abe_priv->port[OMAP_ABE_BE_PORT_DMIC2]);
		break;
	}
}

static void disable_fe_port(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);

	dev_dbg(&fe->dev, "%s: %s %d\n", __func__, dai->name, stream);

	switch(dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_MM_DL1]);
		else
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_MM_UL1]);
		break;
	case ABE_FRONTEND_DAI_LP_MEDIA:
		abe_disable_data_transfer(MM_DL_PORT);
		break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_MM_UL2]);
		break;
	case ABE_FRONTEND_DAI_MODEM:
	case ABE_FRONTEND_DAI_VOICE:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_VX_DL]);
		else
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_VX_UL]);
		break;
	case ABE_FRONTEND_DAI_TONES:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_TONES]);
		break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			omap_abe_port_disable(abe_priv->abe,
					abe_priv->port[OMAP_ABE_FE_PORT_VIB]);
		break;
	}
}

static void mute_fe_port(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);

	dev_dbg(&fe->dev, "%s: %s %d\n", __func__, dai->name, stream);

	switch(dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
	case ABE_FRONTEND_DAI_LP_MEDIA:
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL2]))
			abe_mute_gain(MIXDL2, MIX_DL2_INPUT_MM_DL);
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL1]))
			abe_mute_gain(MIXDL1, MIX_DL1_INPUT_MM_DL);
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL2]))
			abe_mute_gain(MIXDL2, MIX_DL2_INPUT_VX_DL);
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL1]))
			abe_mute_gain(MIXDL1, MIX_DL1_INPUT_VX_DL);
		break;
	case ABE_FRONTEND_DAI_TONES:
			if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL2]))
			abe_mute_gain(MIXDL2, MIX_DL2_INPUT_TONES);
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL1]))
			abe_mute_gain(MIXDL1, MIX_DL1_INPUT_TONES);
		break;
	case ABE_FRONTEND_DAI_VIBRA:
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		break;
	}
}

static void unmute_fe_port(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);

	dev_dbg(&fe->dev, "%s: %s %d\n", __func__, dai->name, stream);

	switch(dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
	case ABE_FRONTEND_DAI_LP_MEDIA:
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL2]))
			abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_MM_DL);
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL1]))
			abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_MM_DL);
		break;
	case ABE_FRONTEND_DAI_VOICE:
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL2]))
			abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_VX_DL);
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL1]))
			abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_VX_DL);
		break;
	case ABE_FRONTEND_DAI_TONES:
			if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL2]))
			abe_unmute_gain(MIXDL2, MIX_DL2_INPUT_TONES);
		if (omap_abe_port_is_enabled(abe_priv->abe,
						abe_priv->port[OMAP_ABE_BE_PORT_PDM_DL1]))
			abe_unmute_gain(MIXDL1, MIX_DL1_INPUT_TONES);
		break;
	case ABE_FRONTEND_DAI_VIBRA:
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		break;
	}
}

static void capture_trigger(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int cmd)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct snd_soc_dsp_params *dsp_params;
	struct snd_pcm_substream *be_substream;
	int stream = substream->stream;

	dev_dbg(&fe->dev, "%s: %s %d\n", __func__, fe->cpu_dai->name, stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:

		/* mute and enable BE ports */
		list_for_each_entry(dsp_params, &fe->dsp[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dsp_params->be;

			/* does this trigger() apply to this BE and stream ? */
			if (!snd_soc_dsp_is_trigger_for_be(fe, be, stream))
				continue;

			/* is the BE already in the trigger START state ? */
			if (dsp_params->state == SND_SOC_DSP_LINK_STATE_START)
				continue;

			be_substream = snd_soc_dsp_get_substream(dsp_params->be, stream);

			/* mute the BE port */
			mute_be(be, dai, stream);

			/* enable the BE port */
			enable_be_port(be, dai, stream);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* trigger the BE port */
			snd_soc_dai_trigger(be_substream, cmd, be->cpu_dai);
		}

		/* does this trigger() apply to the FE ? */
		if (snd_soc_dsp_is_trigger_for_fe(fe, stream)) {
			/* Enable Frontend sDMA  */
			snd_soc_dsp_platform_trigger(substream, cmd, fe->platform);
			enable_fe_port(substream, dai, stream);
		}

		/* Restore ABE GAINS AMIC */
		list_for_each_entry(dsp_params, &fe->dsp[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dsp_params->be;

			/* does this trigger() apply to this BE and stream ? */
			if (!snd_soc_dsp_is_trigger_for_be(fe, be, stream))
				continue;

			/* unmute this BE port */
			unmute_be(be, dai, stream);
		}
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable sDMA */
		snd_soc_dsp_platform_trigger(substream, cmd, fe->platform);
		enable_fe_port(substream, dai, stream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Disable sDMA */
		disable_fe_port(substream, dai, stream);
		snd_soc_dsp_platform_trigger(substream, cmd, fe->platform);
		break;
	case SNDRV_PCM_TRIGGER_STOP:

		/* does this trigger() apply to the FE ? */
		if (snd_soc_dsp_is_trigger_for_fe(fe, stream)) {
			/* Disable sDMA */
			disable_fe_port(substream, dai, stream);
			snd_soc_dsp_platform_trigger(substream, cmd, fe->platform);
		}

		/* disable BE ports */
		list_for_each_entry(dsp_params, &fe->dsp[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dsp_params->be;

			/* does this trigger() apply to this BE and stream ? */
			if (!snd_soc_dsp_is_trigger_for_be(fe, be, stream))
				continue;

			/* only STOP BE in FREE state */
			/* REVISIT: Investigate the appropriate state to check against */
			//if (dsp_params->state != SND_SOC_DSP_LINK_STATE_FREE)
			//	continue;

			be_substream = snd_soc_dsp_get_substream(dsp_params->be, stream);

			/* disable the BE port */
			disable_be_port(be, dai, stream);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* trigger BE port */
			snd_soc_dai_trigger(be_substream, cmd, be->cpu_dai);
		}
		break;
	default:
		break;
	}
}

static void playback_trigger(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai, int cmd)
{
	struct snd_soc_pcm_runtime *fe = substream->private_data;
	struct snd_soc_dsp_params *dsp_params;
	struct snd_pcm_substream *be_substream;
	int stream = substream->stream;

	dev_dbg(&fe->dev, "%s: %s %d\n", __func__, fe->cpu_dai->name, stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:

		/* mute and enable ports */
		list_for_each_entry(dsp_params, &fe->dsp[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dsp_params->be;

			/* does this trigger() apply to the FE ? */
			if (!snd_soc_dsp_is_trigger_for_be(fe, be, stream))
				continue;

			/* is the BE already in the trigger START state ? */
			if (dsp_params->state == SND_SOC_DSP_LINK_STATE_START)
				continue;

			be_substream = snd_soc_dsp_get_substream(dsp_params->be, stream);

			/* mute BE port */
			mute_be(be, dai, stream);

			/* enabled BE port */
			enable_be_port(be, dai, stream);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/* trigger BE port */
			snd_soc_dai_trigger(be_substream, cmd, be->cpu_dai);

			/* unmute the BE port */
			unmute_be(be, dai, stream);
		}

		/* does this trigger() apply to the FE ? */
		if (snd_soc_dsp_is_trigger_for_fe(fe, stream)) {

			/* Enable Frontend sDMA  */
			snd_soc_dsp_platform_trigger(substream, cmd, fe->platform);
			enable_fe_port(substream, dai, stream);

			/* unmute FE port */
			unmute_fe_port(substream, dai, stream);
		}
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Enable Frontend sDMA  */
		snd_soc_dsp_platform_trigger(substream, cmd, fe->platform);
		enable_fe_port(substream, dai, stream);

		/* unmute FE port */
		unmute_fe_port(substream, dai, stream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* disable Frontend sDMA  */
		disable_fe_port(substream, dai, stream);
		snd_soc_dsp_platform_trigger(substream, cmd, fe->platform);

		/* mute FE port */
		mute_fe_port(substream, dai, stream);
		break;
	case SNDRV_PCM_TRIGGER_STOP:

		/* does this trigger() apply to the FE ? */
		if (snd_soc_dsp_is_trigger_for_fe(fe, stream)) {

			/* disable the transfer */
			disable_fe_port(substream, dai, stream);
			snd_soc_dsp_platform_trigger(substream, cmd, fe->platform);

			/* mute FE port */
			mute_fe_port(substream, dai, stream);
		}

		/* disable BE ports */
		list_for_each_entry(dsp_params, &fe->dsp[stream].be_clients, list_be) {
			struct snd_soc_pcm_runtime *be = dsp_params->be;

			/* does this trigger() apply to this BE and stream ? */
			if (!snd_soc_dsp_is_trigger_for_be(fe, be, stream))
				continue;

			/* only STOP BE in FREE state */
			if (dsp_params->state != SND_SOC_DSP_LINK_STATE_FREE)
				continue;

			be_substream = snd_soc_dsp_get_substream(dsp_params->be, stream);

			/* disable the BE */
			disable_be_port(be, dai, stream);

			/* DAI work must be started/stopped at least 250us after ABE */
			udelay(250);

			/*  trigger the BE port */
			snd_soc_dai_trigger(be_substream, cmd, be->cpu_dai);
		}
		break;
	default:
		break;
	}
}

static int omap_abe_dai_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {

		ret = modem_get_dai(substream, dai);
		if (ret < 0) {
			dev_err(dai->dev, "failed to get MODEM DAI\n");
			return ret;
		}
		dev_dbg(abe_priv->modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		ret = snd_soc_dai_startup(abe_priv->modem_substream[substream->stream],
				abe_priv->modem_dai);
		if (ret < 0) {
			dev_err(abe_priv->modem_dai->dev, "failed to open DAI %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int omap_abe_dai_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);
	abe_data_format_t format;
	abe_dma_t dma_sink;
	abe_dma_t dma_params;
	int ret;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	switch (params_channels(params)) {
	case 1:
		if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
			format.samp_format = MONO_RSHIFTED_16;
		else
			format.samp_format = MONO_MSB;
		break;
	case 2:
		if (params_format(params) == SNDRV_PCM_FORMAT_S16_LE)
			format.samp_format = STEREO_16_16;
		else
			format.samp_format = STEREO_MSB;
		break;
	case 3:
		format.samp_format = THREE_MSB;
		break;
	case 4:
		format.samp_format = FOUR_MSB;
		break;
	case 5:
		format.samp_format = FIVE_MSB;
		break;
	case 6 :
		format.samp_format = SIX_MSB;
		break;
	case 7 :
		format.samp_format = SEVEN_MSB;
		break;
	case 8:
		format.samp_format = EIGHT_MSB;
		break;
	default:
		dev_err(dai->dev, "%d channels not supported",
			params_channels(params));
		return -EINVAL;
	}

	format.f = params_rate(params);

	switch (dai->id) {
	case ABE_FRONTEND_DAI_MEDIA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_connect_cbpr_dmareq_port(MM_DL_PORT, &format, ABE_CBPR0_IDX,
					&dma_sink);
			abe_read_port_address(MM_DL_PORT, &dma_params);
		} else {
			abe_connect_cbpr_dmareq_port(MM_UL_PORT, &format,  ABE_CBPR3_IDX,
					&dma_sink);
			abe_read_port_address(MM_UL_PORT, &dma_params);
		}
        break;
	case ABE_FRONTEND_DAI_LP_MEDIA:
		return 0;
	break;
	case ABE_FRONTEND_DAI_MEDIA_CAPTURE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			return -EINVAL;
		else {
			abe_connect_cbpr_dmareq_port(MM_UL2_PORT, &format,  ABE_CBPR4_IDX,
					&dma_sink);
			abe_read_port_address(MM_UL2_PORT, &dma_params);
		}
        break;
	case ABE_FRONTEND_DAI_VOICE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_connect_cbpr_dmareq_port(VX_DL_PORT, &format, ABE_CBPR1_IDX,
					&dma_sink);
			abe_read_port_address(VX_DL_PORT, &dma_params);
		} else {
			abe_connect_cbpr_dmareq_port(VX_UL_PORT, &format,  ABE_CBPR2_IDX,
					&dma_sink);
			abe_read_port_address(VX_UL_PORT, &dma_params);
		}
        break;
	case ABE_FRONTEND_DAI_TONES:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_connect_cbpr_dmareq_port(TONES_DL_PORT, &format, ABE_CBPR5_IDX,
					&dma_sink);
			abe_read_port_address(TONES_DL_PORT, &dma_params);
		} else
			return -EINVAL;
        break;
	case ABE_FRONTEND_DAI_VIBRA:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			abe_connect_cbpr_dmareq_port(VIB_DL_PORT, &format, ABE_CBPR6_IDX,
					&dma_sink);
			abe_read_port_address(VIB_DL_PORT, &dma_params);
		} else
			return -EINVAL;
		break;
	case ABE_FRONTEND_DAI_MODEM:
		/* MODEM is special case where data IO is performed by McBSP2
		 * directly onto VX_DL and VX_UL (instead of SDMA).
		 */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			/* Vx_DL connection to McBSP 2 ports */
			format.samp_format = STEREO_RSHIFTED_16;
			abe_connect_serial_port(VX_DL_PORT, &format, MCBSP2_RX);
			abe_read_port_address(VX_DL_PORT, &dma_params);
		} else {
			/* Vx_UL connection to McBSP 2 ports */
			format.samp_format = STEREO_RSHIFTED_16;
			abe_connect_serial_port(VX_UL_PORT, &format, MCBSP2_TX);
			abe_read_port_address(VX_UL_PORT, &dma_params);
		}
        break;
	}

	/* configure frontend SDMA data */
	omap_abe_dai_dma_params[dai->id][substream->stream].port_addr =
			(unsigned long)dma_params.data;
	omap_abe_dai_dma_params[dai->id][substream->stream].packet_size =
			dma_params.iter;

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {
		/* call hw_params on McBSP with correct DMA data */
		snd_soc_dai_set_dma_data(abe_priv->modem_dai, substream,
				&omap_abe_dai_dma_params[dai->id][substream->stream]);

		dev_dbg(abe_priv->modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		ret = snd_soc_dai_hw_params(abe_priv->modem_substream[substream->stream],
				params, abe_priv->modem_dai);
		if (ret < 0)
			dev_err(abe_priv->modem_dai->dev, "MODEM hw_params failed\n");
		return ret;
	}

	snd_soc_dai_set_dma_data(dai, substream,
				&omap_abe_dai_dma_params[dai->id][substream->stream]);

	return 0;
}

static int omap_abe_dai_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {
		ret = snd_soc_dai_prepare(abe_priv->modem_substream[substream->stream],
				abe_priv->modem_dai);

		dev_dbg(abe_priv->modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		if (ret < 0) {
			dev_err(abe_priv->modem_dai->dev, "MODEM prepare failed\n");
			return ret;
		}
	}
	return ret;
}

static int omap_abe_dai_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s cmd %d\n", __func__, dai->name, cmd);

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {

		dev_dbg(abe_priv->modem_dai->dev, "%s: MODEM stream %d cmd %d\n",
				__func__, substream->stream, cmd);

		ret = snd_soc_dai_trigger(abe_priv->modem_substream[substream->stream],
				cmd, abe_priv->modem_dai);
		if (ret < 0) {
			dev_err(abe_priv->modem_dai->dev, "MODEM trigger failed\n");
			return ret;
		}
	}

	return ret;
}

static int omap_abe_dai_bespoke_trigger(struct snd_pcm_substream *substream,
				  int cmd, struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s: %s cmd %d\n", __func__, dai->name, cmd);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		playback_trigger(substream, dai, cmd);
	else
		capture_trigger(substream, dai, cmd);

	return 0;
}

static int omap_abe_dai_hw_free(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {

		dev_dbg(abe_priv->modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		ret = snd_soc_dai_hw_free(abe_priv->modem_substream[substream->stream],
				abe_priv->modem_dai);
		if (ret < 0) {
			dev_err(abe_priv->modem_dai->dev, "MODEM hw_free failed\n");
			return ret;
		}
	}
	return ret;
}

static void omap_abe_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);

	dev_dbg(dai->dev, "%s: %s\n", __func__, dai->name);

	if (dai->id == ABE_FRONTEND_DAI_MODEM) {
		dev_dbg(abe_priv->modem_dai->dev, "%s: MODEM stream %d\n",
				__func__, substream->stream);

		snd_soc_dai_shutdown(abe_priv->modem_substream[substream->stream],
				abe_priv->modem_dai);
	}
}

static int omap_abe_dai_probe(struct snd_soc_dai *dai)
{
	struct omap_abe_data *abe_priv;
	int i;

	abe_priv = kzalloc(sizeof(struct omap_abe_data), GFP_KERNEL);
	if (abe_priv == NULL)
		return -ENOMEM;

	abe_priv->abe = omap_abe_port_mgr_get();
	if (!abe_priv->abe)
		goto err;

	for (i = 0; i <= OMAP_ABE_MAX_PORT_ID; i++) {

		abe_priv->port[i] = omap_abe_port_open(abe_priv->abe, i);
		if (abe_priv->port[i] == NULL) {
			for (--i; i >= 0; i--)
				omap_abe_port_close(abe_priv->abe, abe_priv->port[i]);

			goto err_port;
		}
	}

	snd_soc_dai_set_drvdata(dai, abe_priv);
	return 0;

err_port:
	omap_abe_port_mgr_put(abe_priv->abe);
err:
	kfree(abe_priv);
	return -ENOMEM;
}

static int omap_abe_dai_remove(struct snd_soc_dai *dai)
{
	struct omap_abe_data *abe_priv = snd_soc_dai_get_drvdata(dai);

	omap_abe_port_mgr_put(abe_priv->abe);
	kfree(abe_priv);
	return 0;
}

static struct snd_soc_dai_ops omap_abe_dai_ops = {
	.startup	= omap_abe_dai_startup,
	.shutdown	= omap_abe_dai_shutdown,
	.hw_params	= omap_abe_dai_hw_params,
	.hw_free	= omap_abe_dai_hw_free,
	.prepare	= omap_abe_dai_prepare,
	.trigger	= omap_abe_dai_trigger,
	.bespoke_trigger = omap_abe_dai_bespoke_trigger,
};

static struct snd_soc_dai_driver omap_abe_dai[] = {
	{	/* Multimedia Playback and Capture */
		.name = "MultiMedia1",
		.probe = omap_abe_dai_probe,
		.remove = omap_abe_dai_remove,
		.playback = {
			.stream_name = "MultiMedia1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.capture = {
			.stream_name = "MultiMedia1 Capture",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Multimedia Capture */
		.name = "MultiMedia2",
		.capture = {
			.stream_name = "MultiMedia2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Voice Playback and Capture */
		.name = "Voice",
		.playback = {
			.stream_name = "Voice Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS,
		},
		.capture = {
			.stream_name = "Voice Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Tones Playback */
		.name = "Tones",
		.playback = {
			.stream_name = "Tones Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Vibra */
		.name = "Vibra",
		.playback = {
			.stream_name = "Vibra Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.formats = OMAP_ABE_FORMATS,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* MODEM Voice Playback and Capture */
		.name = "MODEM",
		.playback = {
			.stream_name = "Voice Playback",
			.channels_min = 1,
			.channels_max = 1,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS | SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "Voice Capture",
			.channels_min = 1,
			.channels_max = 1,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = OMAP_ABE_FORMATS | SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &omap_abe_dai_ops,
	},
	{	/* Low Power HiFi Playback */
		.name = "MultiMedia1 LP",
		.playback = {
			.stream_name = "MultiMedia1 LP Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
			.formats = OMAP_ABE_FORMATS | SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &omap_abe_dai_ops,
	},
};

static int __devinit omap_abe_probe(struct platform_device *pdev)
{
	return snd_soc_register_dais(&pdev->dev, omap_abe_dai,
			ARRAY_SIZE(omap_abe_dai));
}

static int __devexit omap_abe_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dais(&pdev->dev, ARRAY_SIZE(omap_abe_dai));
	return 0;
}

static struct platform_driver omap_abe_driver = {
	.driver = {
		.name = "omap-abe-dai",
		.owner = THIS_MODULE,
	},
	.probe = omap_abe_probe,
	.remove = __devexit_p(omap_abe_remove),
};

static int __init omap_abe_init(void)
{
	return platform_driver_register(&omap_abe_driver);
}
module_init(omap_abe_init);

static void __exit omap_abe_exit(void)
{
	platform_driver_unregister(&omap_abe_driver);
}
module_exit(omap_abe_exit);

MODULE_AUTHOR("Liam Girdwood <lrg@ti.com>");
MODULE_DESCRIPTION("OMAP ABE SoC Interface");
MODULE_LICENSE("GPL");
