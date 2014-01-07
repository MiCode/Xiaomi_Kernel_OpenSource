/*
 * Intel SST Haswell/Broadwell PCM Support
 *
 * Copyright (C) 2013, Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/compress_driver.h>

#include "sst-haswell-ipc.h"
#include "sst-dsp-priv.h"
#include "sst-dsp.h"

/*
 * Dont build in the compressed support as it's not required for base FW.
 * We also have to fix an issue on module removal with compressed.
 */
#define HSW_COMPR	0

#define HSW_PCM_COUNT		6
#define HSW_VOLUME_MAX		0x7FFFFFFF	/* 0dB */

/* TODO: to be replaced with windows table */
static const u32 volume_map[] = {
	HSW_VOLUME_MAX >> 30,
	HSW_VOLUME_MAX >> 29,
	HSW_VOLUME_MAX >> 28,
	HSW_VOLUME_MAX >> 27,
	HSW_VOLUME_MAX >> 26,
	HSW_VOLUME_MAX >> 25,
	HSW_VOLUME_MAX >> 24,
	HSW_VOLUME_MAX >> 23,
	HSW_VOLUME_MAX >> 22,
	HSW_VOLUME_MAX >> 21,
	HSW_VOLUME_MAX >> 20,
	HSW_VOLUME_MAX >> 19,
	HSW_VOLUME_MAX >> 18,
	HSW_VOLUME_MAX >> 17,
	HSW_VOLUME_MAX >> 16,
	HSW_VOLUME_MAX >> 15,
	HSW_VOLUME_MAX >> 14,
	HSW_VOLUME_MAX >> 13,
	HSW_VOLUME_MAX >> 12,
	HSW_VOLUME_MAX >> 11,
	HSW_VOLUME_MAX >> 10,
	HSW_VOLUME_MAX >> 9,
	HSW_VOLUME_MAX >> 8,
	HSW_VOLUME_MAX >> 7,
	HSW_VOLUME_MAX >> 6,
	HSW_VOLUME_MAX >> 5,
	HSW_VOLUME_MAX >> 4,
	HSW_VOLUME_MAX >> 3,
	HSW_VOLUME_MAX >> 2,
	HSW_VOLUME_MAX >> 1,
	HSW_VOLUME_MAX >> 0,
};

#define HSW_PCM_PERIODS_MAX	64
#define HSW_PCM_PERIODS_MIN	2

static const struct snd_pcm_hardware hsw_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_NO_PERIOD_WAKEUP,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FORMAT_S24_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min	= PAGE_SIZE,
	.period_bytes_max	= (HSW_PCM_PERIODS_MAX / HSW_PCM_PERIODS_MIN) * PAGE_SIZE,
	.periods_min		= HSW_PCM_PERIODS_MIN,
	.periods_max		= HSW_PCM_PERIODS_MAX,
	.buffer_bytes_max	= HSW_PCM_PERIODS_MAX * PAGE_SIZE,
};

/* private data for each PCM DSP stream */
struct hsw_pcm_data {
	int dai_id;
	struct sst_hsw_stream *stream;
	u32 volume[2];
	struct snd_pcm_substream *substream;
	struct snd_compr_stream *cstream;
	unsigned int wpos;
	struct mutex mutex;
};

/* private data for the driver */
struct hsw_priv_data {
	/* runtime DSP */
	struct sst_hsw *hsw;

	/* page tables */
	unsigned char *pcm_pg[HSW_PCM_COUNT][2];

	/* DAI data */
	struct hsw_pcm_data pcm[HSW_PCM_COUNT];
};

static inline u32 hsw_mixer_to_ipc(unsigned int value)
{
	if (value >= ARRAY_SIZE(volume_map))
		return volume_map[0];
	else
		return volume_map[value];
}

static inline unsigned int hsw_ipc_to_mixer(u32 value)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(volume_map); i++) {
		if (volume_map[i] >= value)
			return i;
	}
	return i - 1;
}

static int hsw_stream_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(platform);
	struct hsw_pcm_data *pcm_data = &pdata->pcm[mc->reg];
	struct sst_hsw *hsw = pdata->hsw;
	u32 volume;

	mutex_lock(&pcm_data->mutex);

	if (!pcm_data->stream) {
		pcm_data->volume[0] = hsw_mixer_to_ipc(ucontrol->value.integer.value[0]);
		pcm_data->volume[1] = hsw_mixer_to_ipc(ucontrol->value.integer.value[1]);
		mutex_unlock(&pcm_data->mutex);
		return 0;
	}

	if (ucontrol->value.integer.value[0] ==
		ucontrol->value.integer.value[1]) {
		volume = hsw_mixer_to_ipc(ucontrol->value.integer.value[0]);
		sst_hsw_stream_set_volume(hsw, pcm_data->stream, 0, 2, volume);
	} else {
		volume = hsw_mixer_to_ipc(ucontrol->value.integer.value[0]);
		sst_hsw_stream_set_volume(hsw, pcm_data->stream, 0, 0, volume);
		volume = hsw_mixer_to_ipc(ucontrol->value.integer.value[1]);
		sst_hsw_stream_set_volume(hsw, pcm_data->stream, 0, 1, volume);
	}

	mutex_unlock(&pcm_data->mutex);
	return 0;
}

static int hsw_stream_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(platform);
	struct hsw_pcm_data *pcm_data = &pdata->pcm[mc->reg];
	struct sst_hsw *hsw = pdata->hsw;
	u32 volume;

	mutex_lock(&pcm_data->mutex);

	if (!pcm_data->stream) {
		ucontrol->value.integer.value[0] = hsw_ipc_to_mixer(pcm_data->volume[0]);
		ucontrol->value.integer.value[1] = hsw_ipc_to_mixer(pcm_data->volume[1]);
		mutex_unlock(&pcm_data->mutex);
		return 0;
	}

	sst_hsw_stream_get_volume(hsw, pcm_data->stream, 0, 0, &volume);
	ucontrol->value.integer.value[0] = hsw_ipc_to_mixer(volume);
	sst_hsw_stream_get_volume(hsw, pcm_data->stream, 0, 1, &volume);
	ucontrol->value.integer.value[1] = hsw_ipc_to_mixer(volume);
	mutex_unlock(&pcm_data->mutex);

	return 0;
}

static int hsw_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct hsw_priv_data *pdata = snd_soc_platform_get_drvdata(platform);
	struct sst_hsw *hsw = pdata->hsw;
	u32 volume;

	if (ucontrol->value.integer.value[0] ==
		ucontrol->value.integer.value[1]) {

		volume = hsw_mixer_to_ipc(ucontrol->value.integer.value[0]);
		sst_hsw_mixer_set_volume(hsw, 0, 2, volume);

	} else {
		volume = hsw_mixer_to_ipc(ucontrol->value.integer.value[0]);
		sst_hsw_mixer_set_volume(hsw, 0, 0, volume);

		volume = hsw_mixer_to_ipc(ucontrol->value.integer.value[1]);
		sst_hsw_mixer_set_volume(hsw, 0, 1, volume);
	}

	return 0;
}

static int hsw_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct hsw_priv_data *pdata = snd_soc_platform_get_drvdata(platform);
	struct sst_hsw *hsw = pdata->hsw;
	unsigned int volume = 0;

	sst_hsw_mixer_get_volume(hsw, 0, 0, &volume);
	ucontrol->value.integer.value[0] = hsw_ipc_to_mixer(volume);

	sst_hsw_mixer_get_volume(hsw, 0, 1, &volume);
	ucontrol->value.integer.value[1] = hsw_ipc_to_mixer(volume);

	return 0;
}

/* TLV used by both global and stream volumes */
static const DECLARE_TLV_DB_SCALE(hsw_vol_tlv, -9000, 300, 1);

/* System Pin has no volume control */
static const struct snd_kcontrol_new hsw_volume_controls[] = {
	/* Global DSP volume */
	SOC_DOUBLE_EXT_TLV("Master Playback Volume", 0, 0, 8,
		ARRAY_SIZE(volume_map) -1, 0,
		hsw_volume_get, hsw_volume_put, hsw_vol_tlv),
	/* Offload 0 volume */
	SOC_DOUBLE_EXT_TLV("Media0 Playback Volume", 1, 0, 8,
		ARRAY_SIZE(volume_map), 0,
		hsw_stream_volume_get, hsw_stream_volume_put, hsw_vol_tlv),
	/* Offload 1 volume */
	SOC_DOUBLE_EXT_TLV("Media1 Playback Volume", 2, 0, 8,
		ARRAY_SIZE(volume_map), 0,
		hsw_stream_volume_get, hsw_stream_volume_put, hsw_vol_tlv),
	/* Loopback volume */
	SOC_DOUBLE_EXT_TLV("Loopback Capture Volume", 3, 0, 8,
		ARRAY_SIZE(volume_map), 0,
		hsw_stream_volume_get, hsw_stream_volume_put, hsw_vol_tlv),
	/* Mic Capture volume */
	SOC_DOUBLE_EXT_TLV("Mic Capture Volume", 4, 0, 8,
		ARRAY_SIZE(volume_map), 0,
		hsw_stream_volume_get, hsw_stream_volume_put, hsw_vol_tlv),
};

/* Create DMA buffer page table for DSP */
static int create_adsp_page_table(struct hsw_priv_data *pdata,
	struct snd_soc_pcm_runtime *rtd,
	unsigned char *dma_area, size_t size, int pcm, int stream)
{
	int i, pages;

	if (size % PAGE_SIZE)
		pages = (size / PAGE_SIZE) + 1;
	else
		pages = size / PAGE_SIZE;

	dev_dbg(rtd->dev, "generating page table for %p size 0x%zu pages %d\n",
		dma_area, size, pages);

	for (i = 0; i < pages; i++) {
		u32 idx = (((i << 2) + i)) >> 1;
		u32 pfn = (virt_to_phys(dma_area + i * PAGE_SIZE)) >> PAGE_SHIFT;
		u32 *pg_table;

		dev_dbg(rtd->dev, "pfn i %i idx %d pfn %x\n", i, idx, pfn);

		pg_table = (u32*)(pdata->pcm_pg[pcm][stream] + idx);

		if (i & 1)
			*pg_table |= (pfn << 4);
		else
			*pg_table |= pfn;
	}

	return 0;
}

/* this may get called several times by oss emulation */
static int hsw_pcm_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = snd_soc_pcm_get_drvdata(rtd);
	struct sst_hsw *hsw = pdata->hsw;
	struct sst_module *module_data;
	struct sst_dsp *dsp;
	enum sst_hsw_stream_type stream_type;
	enum sst_hsw_stream_path_id path_id;
	u32 rate, bits, map, pages, module_id;
	u8 channels;
	int ret;

	dev_dbg(rtd->dev, "PCM: hw_params, pcm_data %p\n", pcm_data);

	/* stream direction */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		path_id = SST_HSW_STREAM_PATH_SSP0_OUT;
	else
		path_id = SST_HSW_STREAM_PATH_SSP0_IN;

	/* stream type depends on DAI ID */
	switch (rtd->cpu_dai->id) {
	case 0:
		stream_type = SST_HSW_STREAM_TYPE_SYSTEM;
		module_id = SST_HSW_MODULE_PCM_SYSTEM;
		break;
	case 1:
	case 2:
		stream_type = SST_HSW_STREAM_TYPE_RENDER;
		module_id = SST_HSW_MODULE_PCM;
		break;
	case 3:
		/* path ID needs to be OUT for loopback */
		stream_type = SST_HSW_STREAM_TYPE_LOOPBACK;
		path_id = SST_HSW_STREAM_PATH_SSP0_OUT;
		module_id = SST_HSW_MODULE_PCM_REFERENCE;
		break;
	case 4:
		stream_type = SST_HSW_STREAM_TYPE_CAPTURE;
		module_id = SST_HSW_MODULE_PCM_CAPTURE;
		break;
	default:
		dev_err(rtd->dev, "invalid DAI ID %d\n", rtd->cpu_dai->id);
		return -EINVAL;
	};

	ret = sst_hsw_stream_format(hsw, pcm_data->stream,
		path_id, stream_type,
		SST_HSW_STREAM_FORMAT_PCM_FORMAT);
	if (ret < 0) {
		dev_err(rtd->dev, "failed to set stream format %d\n", ret);
		return ret;
	}

	switch (params_rate(params)) {
	case 8000:
		rate = SST_HSW_FS_8000HZ;
		break;
	case 11025:
		rate = SST_HSW_FS_11025HZ;
		break;
	case 12000:
		rate = SST_HSW_FS_12000HZ;
		break;
	case 16000:
		rate = SST_HSW_FS_16000HZ;
		break;
	case 22050:
		rate = SST_HSW_FS_22050HZ;
		break;
	case 24000:
		rate = SST_HSW_FS_24000HZ;
		break;
	case 32000:
		rate = SST_HSW_FS_32000HZ;
		break;
	case 44100:
		rate = SST_HSW_FS_44100HZ;
		break;
	case 48000:
		rate = SST_HSW_FS_48000HZ;
		break;
	case 64000:
		rate = SST_HSW_FS_64000HZ;
		break;
	case 88200:
		rate = SST_HSW_FS_88200HZ;
		break;
	case 96000:
		rate = SST_HSW_FS_96000HZ;
		break;
	case 128000:
		rate = SST_HSW_FS_128000HZ;
		break;
	case 176400:
		rate = SST_HSW_FS_176400HZ;
		break;
	case 192000:
		rate = SST_HSW_FS_192000HZ;
		break;
	default:
		dev_err(rtd->dev, "invalid rate %d\n", params_rate(params));
		return -EINVAL;
	}

	ret = sst_hsw_stream_set_rate(hsw, pcm_data->stream, rate);
	if (ret < 0) {
		dev_err(rtd->dev, "could not set rate %d\n", params_rate(params));
		return ret;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		bits = SST_HSW_DEPTH_16BIT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		bits = SST_HSW_DEPTH_24BIT;
		break;
	case SNDRV_PCM_FORMAT_S8:
		bits = SST_HSW_DEPTH_8BIT;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		bits = SST_HSW_DEPTH_32BIT;
		break;
	default:
		dev_err(rtd->dev, "invalid format %d\n", params_format(params));
		return -EINVAL;
	}

	ret = sst_hsw_stream_set_bits(hsw, pcm_data->stream, bits);
	if (ret < 0) {
		dev_err(rtd->dev, "could not set formats %d\n", params_rate(params));
		return ret;
	}

	channels = (u8)(params_channels(params) & 0xF);
	ret = sst_hsw_stream_set_channels(hsw, pcm_data->stream, channels);
	if (ret < 0) {
		dev_err(rtd->dev, "could not set channels %d\n", params_rate(params));
		return ret;
	}

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	if (ret < 0) {
		dev_err(rtd->dev, "could not allocate %d bytes for PCM %d\n",
			params_buffer_bytes(params), ret);
		return ret;
	}

	ret = create_adsp_page_table(pdata, rtd, runtime->dma_area,
		runtime->dma_bytes, rtd->cpu_dai->id, substream->stream);
	if (ret < 0)
		return ret;

	// TODO leave these hard coded atm
	map = create_channel_map(SST_HSW_CHANNEL_CONFIG_STEREO);
	sst_hsw_stream_set_map_config(hsw, pcm_data->stream,
			map, SST_HSW_CHANNEL_CONFIG_STEREO);
	sst_hsw_stream_set_style(hsw, pcm_data->stream, SST_HSW_INTERLEAVING_PER_CHANNEL);
	sst_hsw_stream_set_valid(hsw, pcm_data->stream, 16);

	if (runtime->dma_bytes % PAGE_SIZE)
		pages = (runtime->dma_bytes / PAGE_SIZE) + 1;
	else
		pages = runtime->dma_bytes / PAGE_SIZE;

	ret = sst_hsw_stream_buffer(hsw, pcm_data->stream,
		virt_to_phys(pdata->pcm_pg[rtd->cpu_dai->id][substream->stream]),
		pages, runtime->dma_bytes, 0,
		(u32)(virt_to_phys(runtime->dma_area) >> PAGE_SHIFT));
	if (ret < 0) {
		dev_err(rtd->dev, "PCM: failed to set DMA buffer %d\n", ret);
		return ret;
	}

	dsp = sst_hsw_get_dsp(hsw);

	module_data = sst_module_get_from_id(dsp, module_id);
	if (module_data == NULL) {
		dev_err(rtd->dev, "PCM: failed to get module config\n");
		return -EINVAL;
	}

	/* TODO: validate module parameters for allocate stream */
	if (stream_type == SST_HSW_STREAM_TYPE_CAPTURE) {
		sst_hsw_stream_set_module_info(hsw, pcm_data->stream,
			SST_HSW_MODULE_PCM_CAPTURE, module_data->entry);
		sst_hsw_stream_set_pmemory_info(hsw, pcm_data->stream,
			0x449400, 0x4000);
		sst_hsw_stream_set_smemory_info(hsw, pcm_data->stream,
			0x400000, 0);
	} else { /* stream_type == SST_HSW_STREAM_TYPE_SYSTEM */
		sst_hsw_stream_set_module_info(hsw, pcm_data->stream,
			SST_HSW_MODULE_PCM_SYSTEM, module_data->entry);

		sst_hsw_stream_set_pmemory_info(hsw, pcm_data->stream,
			module_data->offset, module_data->size);
		sst_hsw_stream_set_pmemory_info(hsw, pcm_data->stream,
			0x44d400, 0x3800);

		sst_hsw_stream_set_smemory_info(hsw, pcm_data->stream,
			module_data->offset, module_data->size);
		sst_hsw_stream_set_smemory_info(hsw, pcm_data->stream,
			0x400000, 0);
	}

	ret = sst_hsw_stream_commit(hsw, pcm_data->stream);
	if (ret < 0) {
		dev_err(rtd->dev, "PCM: failed stream commit %d\n", ret);
		return ret;
	}

	ret = sst_hsw_stream_pause(hsw, pcm_data->stream, 1);
	if (ret < 0)
		dev_err(rtd->dev, "PCM: failed to pause %d after commit\n", ret);

	return 0;
}

static int hsw_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	dev_dbg(rtd->dev, "PCM: hw_free\n");
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int hsw_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = snd_soc_pcm_get_drvdata(rtd);
	struct sst_hsw *hsw = pdata->hsw;

	dev_dbg(rtd->dev, "PCM: trigger %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		sst_hsw_stream_resume(hsw, pcm_data->stream, 0);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		sst_hsw_stream_pause(hsw, pcm_data->stream, 0);
		break;
	default:
		break;
	}

	return 0;
}

static u32 hsw_notify_pointer(struct sst_hsw_stream *stream, void *data)
{
	struct hsw_pcm_data *pcm_data = data;
	struct snd_pcm_substream *substream = pcm_data->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	u32 pos;

	pos = frames_to_bytes(runtime,
		(runtime->control->appl_ptr % runtime->buffer_size));

	dev_dbg(rtd->dev, "PCM: App pointer %d bytes\n", pos);

	/* let alsa know we have play a period */
	snd_pcm_period_elapsed(substream);
	return pos;
}

static snd_pcm_uframes_t hsw_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = snd_soc_pcm_get_drvdata(rtd);
	struct sst_hsw *hsw = pdata->hsw;
	snd_pcm_uframes_t offset;

	offset = bytes_to_frames(runtime,
		sst_hsw_get_dsp_position(hsw, pcm_data->stream));

	dev_dbg(rtd->dev, "PCM: DMA pointer %zu bytes\n",
		frames_to_bytes(runtime, (u32)offset));
	return offset;
}

static int hsw_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = snd_soc_pcm_get_drvdata(rtd);
	struct sst_hsw *hsw = pdata->hsw;

	dev_dbg(rtd->dev, "PCM: open\n");

	pcm_data = &pdata->pcm[rtd->cpu_dai->id];
	mutex_lock(&pcm_data->mutex);

	snd_soc_pcm_set_drvdata(rtd, pcm_data);
	pcm_data->substream = substream;

	snd_soc_set_runtime_hwparams(substream, &hsw_pcm_hardware);

	pcm_data->stream = sst_hsw_stream_new(hsw, rtd->cpu_dai->id,
		hsw_notify_pointer, pcm_data);
	if (pcm_data->stream == NULL) {
		dev_err(rtd->dev, "failed to create stream\n");
		mutex_unlock(&pcm_data->mutex);
		return -EINVAL;
	}

	/* Set previous save volume */
	sst_hsw_stream_set_volume(hsw, pcm_data->stream, 0,
			0, pcm_data->volume[0]);
	sst_hsw_stream_set_volume(hsw, pcm_data->stream, 0,
			1, pcm_data->volume[1]);

	mutex_unlock(&pcm_data->mutex);
	return 0;
}

static int hsw_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = snd_soc_pcm_get_drvdata(rtd);
	struct sst_hsw *hsw = pdata->hsw;
	int ret;

	dev_dbg(rtd->dev, "PCM: close\n");

	mutex_lock(&pcm_data->mutex);
	ret = sst_hsw_stream_reset(hsw, pcm_data->stream);
	if (ret < 0) {
		dev_dbg(rtd->dev, "Reset stream fail!!!\n");
		goto out;
	}

	ret = sst_hsw_stream_free(hsw, pcm_data->stream);
	if (ret < 0) {
		dev_dbg(rtd->dev, "Free stream fail!!!\n");
		goto out;
	}
	pcm_data->stream = NULL;

out:
	mutex_unlock(&pcm_data->mutex);
	return ret;
}

static int hsw_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	dev_dbg(rtd->dev, "PCM: mmap\n");
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops hsw_pcm_ops = {
	.open		= hsw_pcm_open,
	.close		= hsw_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= hsw_pcm_hw_params,
	.hw_free	= hsw_pcm_hw_free,
	.trigger	= hsw_pcm_trigger,
	.pointer	= hsw_pcm_pointer,
	.mmap		= hsw_pcm_mmap,
};

static void hsw_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int hsw_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	ret = dma_coerce_mask_and_coherent(rtd->card->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream ||
			pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = snd_pcm_lib_preallocate_pages_for_all(pcm,
			SNDRV_DMA_TYPE_DEV,
			rtd->card->dev,
			hsw_pcm_hardware.buffer_bytes_max,
			hsw_pcm_hardware.buffer_bytes_max);
		if (ret) {
			dev_err(rtd->dev, "dma buffer allocation failed %d\n",
				ret);
			return ret;
		}
	}

	return ret;
}

#define HSW_FORMATS \
	(SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S16_LE |\
	 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver hsw_dais[] = {
	{
		.name  = "System Pin",
		.playback = {
			.stream_name = "System Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		/* PCM and compressed */
		.name  = "Offload0 Pin",
#if HSW_COMPR
		.compress_dai = 1,
#endif
		.playback = {
			.stream_name = "Offload0 Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = HSW_FORMATS,
		},
	},
	{
		/* PCM and compressed */
		.name  = "Offload1 Pin",
#if HSW_COMPR
		.compress_dai = 1,
#endif
		.playback = {
			.stream_name = "Offload1 Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = HSW_FORMATS,
		},
	},
	{
		.name  = "Loopback Pin",
		.capture = {
			.stream_name = "Loopback Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = HSW_FORMATS,
		},
	},
	{
		.name  = "Capture Pin",
		.capture = {
			.stream_name = "Analog Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = HSW_FORMATS,
		},
	},
};

static const struct snd_soc_dapm_widget widgets[] = {

	/* Backend DAIs  */
	SND_SOC_DAPM_AIF_IN("SSP0 CODEC IN", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SSP0 CODEC OUT", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SSP1 BT IN", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SSP1 BT OUT", NULL, 0, SND_SOC_NOPM, 0, 0),

	/* Global Playback Mixer */
	SND_SOC_DAPM_MIXER("Playback VMixer", SND_SOC_NOPM, 0, 0, NULL, 0),
};

static const struct snd_soc_dapm_route graph[] = {

	/* Playback Mixer */
	{"Playback VMixer", NULL, "System Playback"},
	{"Playback VMixer", NULL, "Offload0 Playback"},
	{"Playback VMixer", NULL, "Offload1 Playback"},

	{"SSP0 CODEC OUT", NULL, "Playback VMixer"},

	{"Analog Capture", NULL, "SSP0 CODEC IN"},
};

static int hsw_pcm_probe(struct snd_soc_platform *platform)
{
	struct sst_pdata *pdata = dev_get_platdata(platform->dev);
	struct hsw_priv_data *priv_data;
	int i;

	if (!pdata)
		return -ENODEV;

	priv_data = devm_kzalloc(platform->dev, sizeof(*priv_data), GFP_KERNEL);
	priv_data->hsw = pdata->dsp;
	snd_soc_platform_set_drvdata(platform, priv_data);

	/* allocate DSP buffer page tables */
	for (i = 0; i < ARRAY_SIZE(hsw_dais); i++) {

		mutex_init(&priv_data->pcm[i].mutex);

		/* playback */
		if (hsw_dais[i].playback.channels_min) {
			priv_data->pcm_pg[i][0] = kzalloc(PAGE_SIZE, GFP_DMA);
			if (priv_data->pcm_pg[i][0] == NULL)
				goto err;
		}

		/* capture */
		if (hsw_dais[i].capture.channels_min) {
			priv_data->pcm_pg[i][1] = kzalloc(PAGE_SIZE, GFP_DMA);
			if (priv_data->pcm_pg[i][1] == NULL)
				goto err;
		}
	}

	return 0;

err:
	for (;i >= 0; i--) {
		if (hsw_dais[i].playback.channels_min)
			kfree(priv_data->pcm_pg[i][0]);
		if (hsw_dais[i].capture.channels_min)
			kfree(priv_data->pcm_pg[i][1]);
	}
	return -ENOMEM;
}

static int hsw_pcm_remove(struct snd_soc_platform *platform)
{
	struct hsw_priv_data *priv_data =
		snd_soc_platform_get_drvdata(platform);
	int i;

	for (i = 0; i < ARRAY_SIZE(hsw_dais); i++) {
		if (hsw_dais[i].playback.channels_min)
			kfree(priv_data->pcm_pg[i][0]);
		if (hsw_dais[i].capture.channels_min)
			kfree(priv_data->pcm_pg[i][1]);
	}

	return 0;
}

#if HSW_COMPR

static u32 hsw_compr_notify_pointer(struct sst_hsw_stream *stream, void *data)
{
	struct hsw_pcm_data *pcm_data = data;
	struct snd_compr_stream *cstream = pcm_data->cstream;

	if (cstream)
		snd_compr_fragment_elapsed(cstream);

	return 0;
}

static int hsw_compr_open(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data;
	struct sst_hsw *hsw = pdata->hsw;

	dev_dbg(rtd->dev, "compr: open\n");

	pcm_data = &pdata->pcm[rtd->cpu_dai->id];

	mutex_lock(&pcm_data->mutex);
	pcm_data->cstream = cstream;
	pcm_data->wpos = 0;

	pcm_data->stream = sst_hsw_stream_new(hsw, rtd->cpu_dai->id,
		 hsw_compr_notify_pointer, pcm_data);
	if (pcm_data->stream == NULL) {
		dev_err(rtd->dev, "failed to create stream\n");
		mutex_unlock(&pcm_data->mutex);
		return -EINVAL;
	}

	runtime->private_data = pcm_data;

	mutex_unlock(&pcm_data->mutex);
	return 0;
}

static int hsw_compr_free(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = runtime->private_data;
	struct sst_hsw *hsw = pdata->hsw;
	int ret;

	dev_dbg(rtd->dev, "compr: close\n");

	mutex_lock(&pcm_data->mutex);

	ret = sst_hsw_stream_reset(hsw, pcm_data->stream);
	if (ret < 0) {
		dev_dbg(rtd->dev, "Reset stream fail!!!\n");
		goto out;
	}

	ret = sst_hsw_stream_free(hsw, pcm_data->stream);
	if (ret < 0) {
		dev_dbg(rtd->dev, "Free stream fail!!!\n");
		goto out;
	}
	pcm_data->stream = NULL;

out:
	mutex_unlock(&pcm_data->mutex);
	return ret;
}

static int hsw_compr_set_params(struct snd_compr_stream *cstream,
	struct snd_compr_params *params)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = runtime->private_data;
	struct sst_hsw *hsw = pdata->hsw;
	enum sst_hsw_stream_path_id path_id;
	enum sst_hsw_stream_format format;
	enum sample_frequency rate;
	u32 map;
	int ret, pages, depth = SST_HSW_DEPTH_INVALID, bits = 0;

	dev_dbg(rtd->dev, "compr: hw_params, pcm_data %p\n", pcm_data);

	/* stream direction -- hard coded atm */
	path_id = SST_HSW_STREAM_PATH_SSP0_OUT;

	/* compressed rate */
	switch (params->codec.sample_rate) {
	case SNDRV_PCM_RATE_44100:
		rate = SST_HSW_FS_44100HZ;
		break;
	case SNDRV_PCM_RATE_48000:
		rate = SST_HSW_FS_48000HZ;
		break;
	default:
		dev_err(rtd->dev, "invalid rate %d\n",
			params->codec.sample_rate);
		return -EINVAL;
	}

	/* stream format - hard code rate atm */
	switch (params->codec.id) {
	case SND_AUDIOCODEC_MP3:
		format = SST_HSW_STREAM_FORMAT_MP3_FORMAT;
		break;
	case SND_AUDIOCODEC_AAC:
		format = SST_HSW_STREAM_FORMAT_AAC_FORMAT;
		break;
	case SND_AUDIOCODEC_PCM:
		format = SST_HSW_STREAM_FORMAT_PCM_FORMAT;
		depth = SST_HSW_DEPTH_16BIT;
		bits = 16;
		break;
	default:
		dev_err(rtd->dev, "invalid compressed format %d\n",
			params->codec.id);
		return -EINVAL;
	}
	ret = sst_hsw_stream_format(hsw, pcm_data->stream,
		path_id, SST_HSW_STREAM_TYPE_RENDER, format);
	if (ret < 0) {
		dev_err(rtd->dev, "failed to set stream format %d\n", ret);
		return ret;
	}

	/* set rate */
	ret = sst_hsw_stream_set_rate(hsw, pcm_data->stream, rate);
	if (ret < 0) {
		dev_err(rtd->dev, "could not set rate %d\n", rate);
		return ret;
	}

	/* set to stereo - TODO hardcoded */
	ret = sst_hsw_stream_set_channels(hsw, pcm_data->stream, 2);
	if (ret < 0) {
		dev_err(rtd->dev, "could not set channels %d\n", 2);
		return ret;
	}

	ret = create_adsp_page_table(pdata, rtd, runtime->buffer,
		runtime->buffer_size, rtd->cpu_dai->id, SNDRV_PCM_STREAM_PLAYBACK);
	if (ret < 0)
		return ret;

	ret = sst_hsw_stream_set_bits(hsw, pcm_data->stream, bits);
	if (ret < 0) {
		dev_err(rtd->dev, "could not set bits %d\n", bits);
		return ret;
	}

	// TODO leave these hard coded atm
	map = create_channel_map(SST_HSW_CHANNEL_CONFIG_STEREO);
	sst_hsw_stream_set_map_config(hsw, pcm_data->stream,
			map, SST_HSW_CHANNEL_CONFIG_STEREO);

	sst_hsw_stream_set_style(hsw, pcm_data->stream, SST_HSW_INTERLEAVING_PER_CHANNEL);
	sst_hsw_stream_set_valid(hsw, pcm_data->stream, depth);

	if (runtime->buffer_size % PAGE_SIZE)
		pages = (runtime->buffer_size / PAGE_SIZE) + 1;
	else
		pages = runtime->buffer_size / PAGE_SIZE;

	ret = sst_hsw_stream_buffer(hsw, pcm_data->stream,
		virt_to_phys(pdata->pcm_pg[rtd->cpu_dai->id][SNDRV_PCM_STREAM_PLAYBACK]),
		pages, runtime->buffer_size, 0,
		(u32)(virt_to_phys(runtime->buffer) >> PAGE_SHIFT));
	if (ret < 0) {
		dev_err(rtd->dev, "compr: failed to set DMA buffer %d\n", ret);
		return ret;
	}

	ret = sst_hsw_stream_commit(hsw, pcm_data->stream);
	if (ret < 0) {
		dev_err(rtd->dev, "compr: failed stream commit %d\n", ret);
		return ret;
	}

	ret = sst_hsw_stream_pause(hsw, pcm_data->stream, 1);
	if (ret < 0)
		dev_err(rtd->dev, "compr: failed to pause %d after commit\n", ret);

	/* Set previous save volume */
	sst_hsw_stream_set_volume(hsw, pcm_data->stream, 0,
			0, pcm_data->volume[0]);
	sst_hsw_stream_set_volume(hsw, pcm_data->stream, 0,
			1, pcm_data->volume[1]);

	return 0;
}

static int hsw_compr_trigger(struct snd_compr_stream *cstream, int cmd)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = runtime->private_data;
	struct sst_hsw *hsw = pdata->hsw;

	dev_dbg(rtd->dev, "compr: trigger %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		sst_hsw_stream_resume(hsw, pcm_data->stream, 0);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		sst_hsw_stream_pause(hsw, pcm_data->stream, 0);
		break;
	default:
		break;
	}

	return 0;
}

static int hsw_compr_pointer(struct snd_compr_stream *cstream,
					struct snd_compr_tstamp *tstamp)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = runtime->private_data;
	struct sst_hsw *hsw = pdata->hsw;
	u32 bytes;

	bytes =	sst_hsw_get_dsp_position(hsw, pcm_data->stream);
	tstamp->pcm_io_frames = bytes * 4; // TODO: hardcoded
	tstamp->copied_total += bytes;
	tstamp->byte_offset = tstamp->copied_total %
				 (u32)cstream->runtime->buffer_size;

	dev_info(rtd->dev, "DSP pointer byte offset 0x%x dsp at frame 0x%zu\n",
		tstamp->byte_offset, tstamp->pcm_io_frames);

	return 0;
}

static int hsw_compr_ack(struct snd_compr_stream *cstream, size_t bytes)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct hsw_priv_data *pdata =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct hsw_pcm_data *pcm_data = runtime->private_data;
	struct sst_hsw *hsw = pdata->hsw;
	int ret;
	u32 pos;

	pcm_data->wpos += bytes;
	pos = pcm_data->wpos % (u32)cstream->runtime->buffer_size;

	ret = sst_hsw_stream_set_write_position(hsw, pcm_data->stream, 0, pos);
	if (ret < 0)
		dev_err(rtd->dev, "compr: failed to set write position to %d\n", ret);

	return ret;
}

static int hsw_compr_get_caps(struct snd_compr_stream *cstream,
					struct snd_compr_caps *caps)
{
	caps->num_codecs = 3;
	caps->min_fragment_size = PAGE_SIZE;
	caps->max_fragment_size = PAGE_SIZE * 1024;
	caps->min_fragments = 2;
	caps->max_fragments = 1024;
	caps->codecs[0] = SND_AUDIOCODEC_MP3;
	caps->codecs[1] = SND_AUDIOCODEC_AAC;
	caps->codecs[2] = SND_AUDIOCODEC_PCM;

	return 0;
}

static int hsw_compr_get_codec_caps(struct snd_compr_stream *cstream,
					struct snd_compr_codec_caps *codec)
{
	switch (codec->codec) {
	case SND_AUDIOCODEC_MP3:
		codec->num_descriptors = 3;
		codec->descriptor[0].max_ch = 2;
		codec->descriptor[0].sample_rates = SNDRV_PCM_RATE_8000_48000;
		codec->descriptor[0].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[0].bit_rate[1] = 192;
		codec->descriptor[0].num_bitrates = 2;
		codec->descriptor[0].profiles = 0;
		codec->descriptor[0].modes = SND_AUDIOCHANMODE_MP3_STEREO;
		codec->descriptor[0].formats = 0;
		break;
	case SND_AUDIOCODEC_AAC:
		codec->num_descriptors = 3;
		codec->descriptor[1].max_ch = 2;
		codec->descriptor[1].sample_rates = SNDRV_PCM_RATE_8000_48000;
		codec->descriptor[1].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[1].bit_rate[1] = 192;
		codec->descriptor[1].num_bitrates = 2;
		codec->descriptor[1].profiles = 0;
		codec->descriptor[1].modes = 0;
		codec->descriptor[1].formats =
			(SND_AUDIOSTREAMFORMAT_MP4ADTS |
				SND_AUDIOSTREAMFORMAT_RAW);
		break;
	case SND_AUDIOCODEC_PCM:
		codec->num_descriptors = 3;
		codec->descriptor[2].max_ch = 2;
		codec->descriptor[2].sample_rates = SNDRV_PCM_RATE_8000_48000;
		codec->descriptor[2].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[2].bit_rate[1] = 192;
		codec->descriptor[2].num_bitrates = 0;
		codec->descriptor[2].profiles = 0;
		codec->descriptor[2].modes = 0;
		codec->descriptor[2].formats = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct snd_compr_ops hsw_compr_ops = {
	.open = hsw_compr_open,
	.free = hsw_compr_free,
	.set_params = hsw_compr_set_params,
	.trigger = hsw_compr_trigger,
	.pointer = hsw_compr_pointer,
	.get_caps = hsw_compr_get_caps,
	.get_codec_caps = hsw_compr_get_codec_caps,
	.ack = hsw_compr_ack,
};
#endif

static struct snd_soc_platform_driver hsw_soc_platform = {
	.probe		= hsw_pcm_probe,
	.remove		= hsw_pcm_remove,
	.ops		= &hsw_pcm_ops,
#if HSW_COMPR
	.compr_ops	= &hsw_compr_ops,
#endif
	.pcm_new	= hsw_pcm_new,
	.pcm_free	= hsw_pcm_free,
	.controls	= hsw_volume_controls,
	.num_controls	= ARRAY_SIZE(hsw_volume_controls),
	.dapm_widgets	= widgets,
	.num_dapm_widgets	= ARRAY_SIZE(widgets),
	.dapm_routes	= graph,
	.num_dapm_routes	= ARRAY_SIZE(graph),
};

static const struct snd_soc_component_driver hsw_dai_component = {
	.name		= "haswell-dai",
};

static int hsw_pcm_dev_probe(struct platform_device *pdev)
{
	struct sst_pdata *sst_pdata = dev_get_platdata(&pdev->dev);
	int ret;

	ret = sst_hsw_dsp_init(&pdev->dev, sst_pdata);
	if (ret < 0)
		return -ENODEV;

	ret = snd_soc_register_platform(&pdev->dev, &hsw_soc_platform);
	if (ret < 0)
		goto err_plat;

	ret = snd_soc_register_component(&pdev->dev, &hsw_dai_component,
		hsw_dais, ARRAY_SIZE(hsw_dais));
	if (ret < 0)
		goto err_comp;

	return 0;

err_comp:
	snd_soc_unregister_platform(&pdev->dev);
err_plat:
	sst_hsw_dsp_free(&pdev->dev, sst_pdata);
	return 0;
}

static int hsw_pcm_dev_remove(struct platform_device *pdev)
{
	struct sst_pdata *sst_pdata = dev_get_platdata(&pdev->dev);

	snd_soc_unregister_platform(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);
	sst_hsw_dsp_free(&pdev->dev, sst_pdata);

	return 0;
}

static struct platform_driver hsw_pcm_driver = {
	.driver = {
		.name = "haswell-pcm-audio",
		.owner = THIS_MODULE,
	},

	.probe = hsw_pcm_dev_probe,
	.remove = hsw_pcm_dev_remove,
};
module_platform_driver(hsw_pcm_driver);

MODULE_AUTHOR("Liam Girdwood, Xingchao Wang");
MODULE_DESCRIPTION("Haswell/Lynxpoint + Broadwell/Wildcatpoint PCM");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:haswell-pcm-audio");
