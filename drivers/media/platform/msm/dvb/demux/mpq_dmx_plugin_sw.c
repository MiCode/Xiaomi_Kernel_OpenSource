/* Copyright (c) 2012,2015, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"


static int mpq_sw_dmx_start_filtering(struct dvb_demux_feed *feed)
{
	int ret = -EINVAL;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	MPQ_DVB_DBG_PRINT("%s(pid=%d) executed\n", __func__, feed->pid);

	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid mpq_demux handle\n", __func__);
		goto out;
	}

	if (mpq_demux->source < DMX_SOURCE_DVR0) {
		MPQ_DVB_ERR_PRINT("%s: only DVR source is supported (%d)\n",
			__func__, mpq_demux->source);
		goto out;
	}

	/*
	 * Always feed sections/PES starting from a new one and
	 * do not partial transfer data from older one
	 */
	feed->pusi_seen = 0;

	ret = mpq_dmx_init_mpq_feed(feed);
	if (ret)
		MPQ_DVB_ERR_PRINT("%s: mpq_dmx_init_mpq_feed failed(%d)\n",
			__func__, ret);
out:
	return ret;
}

static int mpq_sw_dmx_stop_filtering(struct dvb_demux_feed *feed)
{
	int ret;

	MPQ_DVB_DBG_PRINT("%s(%d) executed\n", __func__, feed->pid);

	ret = mpq_dmx_terminate_feed(feed);
	if (ret)
		MPQ_DVB_ERR_PRINT("%s: mpq_dmx_terminate_feed failed(%d)\n",
			__func__, ret);

	return ret;
}

static int mpq_sw_dmx_write_to_decoder(struct dvb_demux_feed *feed,
		const u8 *buf, size_t len)
{
	/*
	 * It is assumed that this function is called once for each
	 * TS packet of the relevant feed.
	 */
	if (len > (TIMESTAMP_LEN + TS_PACKET_SIZE))
		MPQ_DVB_DBG_PRINT(
				"%s: warnning - len larger than one packet\n",
				__func__);

	if (dvb_dmx_is_video_feed(feed))
		return mpq_dmx_process_video_packet(feed, buf);

	if (dvb_dmx_is_pcr_feed(feed))
		return mpq_dmx_process_pcr_packet(feed, buf);

	return 0;
}

static int mpq_sw_dmx_set_source(struct dmx_demux *demux,
		const dmx_source_t *src)
{
	int ret = -EINVAL;

	if (demux == NULL || demux->priv == NULL || src == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		goto out;
	}

	if (*src >= DMX_SOURCE_DVR0 && *src <= DMX_SOURCE_DVR3) {
		ret = mpq_dmx_set_source(demux, src);
		if (ret)
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_set_source(%d) failed, ret=%d\n",
				__func__, *src, ret);
	} else {
		MPQ_DVB_ERR_PRINT("%s: not a DVR source\n", __func__);
	}

out:
	return ret;
}

static int mpq_sw_dmx_get_caps(struct dmx_demux *demux, struct dmx_caps *caps)
{
	struct dvb_demux *dvb_demux = demux->priv;

	if (dvb_demux == NULL || caps == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	caps->caps = DMX_CAP_PULL_MODE | DMX_CAP_VIDEO_DECODER_DATA |
		DMX_CAP_TS_INSERTION | DMX_CAP_VIDEO_INDEXING |
		DMX_CAP_AUTO_BUFFER_FLUSH;
	caps->recording_max_video_pids_indexed = 0;
	caps->num_decoders = MPQ_ADAPTER_MAX_NUM_OF_INTERFACES;
	caps->num_demux_devices = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
	caps->num_pid_filters = MPQ_MAX_DMX_FILES;
	caps->num_section_filters = dvb_demux->filternum;
	caps->num_section_filters_per_pid = dvb_demux->filternum;
	caps->section_filter_length = DMX_FILTER_SIZE;
	caps->num_demod_inputs = 0;
	caps->num_memory_inputs = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
	caps->max_bitrate = 192;
	caps->demod_input_max_bitrate = 96;
	caps->memory_input_max_bitrate = 96;
	caps->num_cipher_ops = 1;

	/* No STC support */
	caps->max_stc = 0;

	/* Buffer requirements */
	caps->section.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT |
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->section.max_buffer_num = 1;
	caps->section.max_size = 0xFFFFFFFF;
	caps->section.size_alignment = 0;
	caps->pes.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT |
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->pes.max_buffer_num = 1;
	caps->pes.max_size = 0xFFFFFFFF;
	caps->pes.size_alignment = 0;
	caps->recording_188_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT |
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->recording_188_tsp.max_buffer_num = 1;
	caps->recording_188_tsp.max_size = 0xFFFFFFFF;
	caps->recording_188_tsp.size_alignment = 0;
	caps->recording_192_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT |
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->recording_192_tsp.max_buffer_num = 1;
	caps->recording_192_tsp.max_size = 0xFFFFFFFF;
	caps->recording_192_tsp.size_alignment = 0;
	caps->playback_188_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT |
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->playback_188_tsp.max_buffer_num = 1;
	caps->playback_188_tsp.max_size = 0xFFFFFFFF;
	caps->playback_188_tsp.size_alignment = 188;
	caps->playback_192_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT |
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->playback_192_tsp.max_buffer_num = 1;
	caps->playback_192_tsp.max_size = 0xFFFFFFFF;
	caps->playback_192_tsp.size_alignment = 192;
	caps->decoder.flags =
		DMX_BUFFER_SECURED_IF_DECRYPTED	|
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT	|
		DMX_BUFFER_LINEAR_GROUP_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->decoder.max_buffer_num = DMX_MAX_DECODER_BUFFER_NUM;
	caps->decoder.max_size = 0xFFFFFFFF;
	caps->decoder.size_alignment = SZ_4K;

	return 0;
}

static int mpq_sw_dmx_init(struct dvb_adapter *mpq_adapter,
		struct mpq_demux *mpq_demux)
{
	int ret;
	struct dvb_demux *dvb_demux = &mpq_demux->demux;

	/* Set the kernel-demux object capabilities */
	mpq_demux->demux.dmx.capabilities =
		DMX_TS_FILTERING			|
		DMX_PES_FILTERING			|
		DMX_SECTION_FILTERING			|
		DMX_MEMORY_BASED_FILTERING		|
		DMX_CRC_CHECKING			|
		DMX_TS_DESCRAMBLING;

	mpq_demux->decoder_alloc_flags = ION_FLAG_CACHED;

	/* Set dvb-demux "virtual" function pointers */
	dvb_demux->priv = (void *)mpq_demux;
	dvb_demux->filternum = MPQ_MAX_DMX_FILES;
	dvb_demux->feednum = MPQ_MAX_DMX_FILES;
	dvb_demux->start_feed = mpq_sw_dmx_start_filtering;
	dvb_demux->stop_feed = mpq_sw_dmx_stop_filtering;
	dvb_demux->write_to_decoder = mpq_sw_dmx_write_to_decoder;
	dvb_demux->decoder_fullness_init = mpq_dmx_decoder_fullness_init;
	dvb_demux->decoder_fullness_wait = mpq_dmx_decoder_fullness_wait;
	dvb_demux->decoder_fullness_abort = mpq_dmx_decoder_fullness_abort;
	dvb_demux->decoder_buffer_status = mpq_dmx_decoder_buffer_status;
	dvb_demux->reuse_decoder_buffer = mpq_dmx_reuse_decoder_buffer;
	dvb_demux->set_cipher_op = mpq_dmx_set_cipher_ops;
	dvb_demux->oob_command = mpq_dmx_oob_command;
	dvb_demux->convert_ts = mpq_dmx_convert_tts;
	dvb_demux->flush_decoder_buffer = NULL;

	/* Initialize dvb_demux object */
	ret = dvb_dmx_init(dvb_demux);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: dvb_dmx_init failed, ret=%d\n",
			__func__, ret);
		goto init_failed;
	}

	/* Now initialize the dmx-dev object */
	mpq_demux->dmxdev.filternum = MPQ_MAX_DMX_FILES;
	mpq_demux->dmxdev.demux = &mpq_demux->demux.dmx;
	mpq_demux->dmxdev.capabilities = DMXDEV_CAP_DUPLEX;

	mpq_demux->dmxdev.demux->set_source = mpq_sw_dmx_set_source;
	mpq_demux->dmxdev.demux->get_stc = NULL;
	mpq_demux->dmxdev.demux->get_caps = mpq_sw_dmx_get_caps;
	mpq_demux->dmxdev.demux->map_buffer = mpq_dmx_map_buffer;
	mpq_demux->dmxdev.demux->unmap_buffer = mpq_dmx_unmap_buffer;
	mpq_demux->dmxdev.demux->write = mpq_dmx_write;
	ret = dvb_dmxdev_init(&mpq_demux->dmxdev, mpq_adapter);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: dvb_dmxdev_init failed, ret=%d\n",
			__func__, ret);
		goto init_failed_dmx_release;
	}

	/* Extend dvb-demux debugfs with mpq demux statistics. */
	mpq_dmx_init_debugfs_entries(mpq_demux);

	return 0;

init_failed_dmx_release:
	dvb_dmx_release(dvb_demux);
init_failed:
	return ret;
}

static int __init mpq_dmx_sw_plugin_init(void)
{
	return mpq_dmx_plugin_init(mpq_sw_dmx_init);
}

static void __exit mpq_dmx_sw_plugin_exit(void)
{
	mpq_dmx_plugin_exit();
}


module_init(mpq_dmx_sw_plugin_init);
module_exit(mpq_dmx_sw_plugin_exit);

MODULE_DESCRIPTION("Qualcomm Technologies Inc. demux software plugin");
MODULE_LICENSE("GPL v2");
