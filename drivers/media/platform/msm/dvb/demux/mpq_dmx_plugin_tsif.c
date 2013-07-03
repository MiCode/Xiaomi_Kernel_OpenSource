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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/tsif_api.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"


/* TSIF HW configuration: */
#define TSIF_COUNT				2

/* Max number of section filters */
#define DMX_TSIF_MAX_SECTION_FILTER_NUM	64

/* When TSIF driver notifies demux that new packets are received */
#define DMX_TSIF_PACKETS_IN_CHUNK_DEF		512
#define DMX_TSIF_CHUNKS_IN_BUF			16
#define DMX_TSIF_TIME_LIMIT			10000

/* TSIF_DRIVER_MODE: 3 means manual control from debugfs. use 2 normally. */
#define DMX_TSIF_DRIVER_MODE_DEF		2

/* module parameters for load time configuration: */
static int threshold = DMX_TSIF_PACKETS_IN_CHUNK_DEF;
static int tsif_mode = DMX_TSIF_DRIVER_MODE_DEF;
static int clock_inv;
module_param(threshold, int, S_IRUGO);
module_param(tsif_mode, int, S_IRUGO | S_IWUSR);
module_param(clock_inv, int, S_IRUGO | S_IWUSR);


/*
 * TSIF driver information
 */
struct tsif_driver_info {
	/* handler to TSIF driver */
	void *tsif_handler;
	/* TSIF driver data buffer pointer */
	void *data_buffer;
	/* TSIF driver data buffer size, in packets */
	int buffer_size;
	/* TSIF driver read pointer */
	int ri;
	/* TSIF driver write pointer */
	int wi;
	/* TSIF driver state */
	enum tsif_state state;
};


/*
 * The following structure hold singelton information
 * required for dmx implementation on top of TSIF.
 */
static struct
{
	/* Information for each TSIF input processing */
	struct {
		/* thread processing TS packets from TSIF */
		struct task_struct *thread;
		wait_queue_head_t wait_queue;

		/* Counter for data notifications from TSIF */
		atomic_t data_cnt;

		/* TSIF alias */
		char name[TSIF_NAME_LENGTH];

		/* TSIF driver information */
		struct tsif_driver_info tsif_driver;

		/* TSIF reference count (counts start/stop operations */
		int ref_count;

		/* Pointer to the demux connected to this TSIF */
		struct mpq_demux *mpq_demux;

		/* mutex protecting the data-structure */
		struct mutex mutex;
	} tsif[TSIF_COUNT];
} mpq_dmx_tsif_info;


/**
 * Demux thread function handling data from specific TSIF.
 *
 * @arg: TSIF number
 */
static int mpq_dmx_tsif_thread(void *arg)
{
	struct mpq_demux *mpq_demux;
	struct tsif_driver_info *tsif_driver;
	size_t packets = 0;
	int tsif = (int)arg;
	int ret;

	do {
		ret = wait_event_interruptible(
			mpq_dmx_tsif_info.tsif[tsif].wait_queue,
			(atomic_read(
				&mpq_dmx_tsif_info.tsif[tsif].data_cnt) != 0) ||
			kthread_should_stop());

		if ((ret < 0) || kthread_should_stop()) {
			MPQ_DVB_DBG_PRINT("%s: exit\n", __func__);
			break;
		}

		if (mutex_lock_interruptible(
			&mpq_dmx_tsif_info.tsif[tsif].mutex))
			return -ERESTARTSYS;

		tsif_driver = &(mpq_dmx_tsif_info.tsif[tsif].tsif_driver);
		mpq_demux = mpq_dmx_tsif_info.tsif[tsif].mpq_demux;

		/* Check if driver handler is still valid */
		if (tsif_driver->tsif_handler == NULL) {
			mutex_unlock(&mpq_dmx_tsif_info.tsif[tsif].mutex);
			MPQ_DVB_DBG_PRINT(
				"%s: tsif was detached\n",
				__func__);
			continue;
		}

		tsif_get_state(
			tsif_driver->tsif_handler, &(tsif_driver->ri),
			&(tsif_driver->wi), &(tsif_driver->state));

		if ((tsif_driver->wi == tsif_driver->ri) ||
			(tsif_driver->state == tsif_state_stopped) ||
			(tsif_driver->state == tsif_state_error)) {

			mpq_demux->hw_notification_size = 0;

			mutex_unlock(&mpq_dmx_tsif_info.tsif[tsif].mutex);

			MPQ_DVB_DBG_PRINT(
				"%s: TSIF invalid state %d, %d, %d\n",
				__func__,
				tsif_driver->state,
				tsif_driver->wi,
				tsif_driver->ri);
			continue;
		}

		atomic_dec(&mpq_dmx_tsif_info.tsif[tsif].data_cnt);

		if (tsif_driver->wi > tsif_driver->ri) {
			packets = (tsif_driver->wi - tsif_driver->ri);
			mpq_demux->hw_notification_size = packets;

			dvb_dmx_swfilter_format(
				&mpq_demux->demux,
				(tsif_driver->data_buffer +
				(tsif_driver->ri * TSIF_PKT_SIZE)),
				(packets * TSIF_PKT_SIZE),
				DMX_TSP_FORMAT_192_TAIL);

			tsif_driver->ri =
				(tsif_driver->ri + packets) %
				tsif_driver->buffer_size;

			tsif_reclaim_packets(
				tsif_driver->tsif_handler,
					tsif_driver->ri);
		} else {
			/*
			 * wi < ri, means wraparound on cyclic buffer.
			 * Handle in two stages.
			 */
			packets = (tsif_driver->buffer_size - tsif_driver->ri);
			mpq_demux->hw_notification_size = packets;

			dvb_dmx_swfilter_format(
				&mpq_demux->demux,
				(tsif_driver->data_buffer +
				(tsif_driver->ri * TSIF_PKT_SIZE)),
				(packets * TSIF_PKT_SIZE),
				DMX_TSP_FORMAT_192_TAIL);

			/* tsif_driver->ri should be 0 after this */
			tsif_driver->ri =
				(tsif_driver->ri + packets) %
				tsif_driver->buffer_size;

			packets = tsif_driver->wi;
			if (packets > 0) {
				mpq_demux->hw_notification_size += packets;

				dvb_dmx_swfilter_format(
					&mpq_demux->demux,
					(tsif_driver->data_buffer +
					(tsif_driver->ri * TSIF_PKT_SIZE)),
					(packets * TSIF_PKT_SIZE),
					DMX_TSP_FORMAT_192_TAIL);

				tsif_driver->ri =
					(tsif_driver->ri + packets) %
					tsif_driver->buffer_size;
			}

			tsif_reclaim_packets(
				tsif_driver->tsif_handler,
				tsif_driver->ri);
		}

		mutex_unlock(&mpq_dmx_tsif_info.tsif[tsif].mutex);
	} while (1);

	return 0;
}


/**
 * Callback function from TSIF driver when new data is ready.
 *
 * @user: user-data holding TSIF number
 */
static void mpq_tsif_callback(void *user)
{
	int tsif = (int)user;
	struct mpq_demux *mpq_demux;

	MPQ_DVB_DBG_PRINT("%s executed, tsif = %d\n", __func__,	tsif);

	/* Save statistics on TSIF notifications */
	mpq_demux = mpq_dmx_tsif_info.tsif[tsif].mpq_demux;
	mpq_dmx_update_hw_statistics(mpq_demux);

	atomic_inc(&mpq_dmx_tsif_info.tsif[tsif].data_cnt);
	wake_up(&mpq_dmx_tsif_info.tsif[tsif].wait_queue);
}


/**
 * Attach to TSIF driver and start TSIF operation.
 *
 * @mpq_demux: the mpq_demux we are working on.
 *
 * Return	error code.
 */
static int mpq_tsif_dmx_start(struct mpq_demux *mpq_demux)
{
	int ret = 0;
	int tsif;
	struct tsif_driver_info *tsif_driver;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	/* determine the TSIF we are reading from */
	if (mpq_demux->source == DMX_SOURCE_FRONT0) {
		tsif = 0;
	} else if (mpq_demux->source == DMX_SOURCE_FRONT1) {
		tsif = 1;
	} else {
		/* invalid source */
		MPQ_DVB_ERR_PRINT(
			"%s: invalid input source (%d)\n",
			__func__,
			mpq_demux->source);

		return -EINVAL;
	}

	if (mutex_lock_interruptible(&mpq_dmx_tsif_info.tsif[tsif].mutex))
		return -ERESTARTSYS;

	if (mpq_dmx_tsif_info.tsif[tsif].ref_count == 0) {
		tsif_driver = &(mpq_dmx_tsif_info.tsif[tsif].tsif_driver);

		/* Attach to TSIF driver */
		tsif_driver->tsif_handler =
			tsif_attach(tsif, mpq_tsif_callback, (void *)tsif);
		if (IS_ERR_OR_NULL(tsif_driver->tsif_handler)) {
			tsif_driver->tsif_handler = NULL;
			mutex_unlock(&mpq_dmx_tsif_info.tsif[tsif].mutex);
			MPQ_DVB_DBG_PRINT("%s: tsif_attach(%d) failed\n",
					__func__, tsif);
			return -ENODEV;
		}

		ret = tsif_set_clk_inverse(tsif_driver->tsif_handler,
					clock_inv);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: tsif_set_clk_inverse (%d) failed\n",
				__func__, clock_inv);
		}

		/* Set TSIF driver mode */
		ret = tsif_set_mode(tsif_driver->tsif_handler, tsif_mode);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT("%s: tsif_set_mode (%d) failed\n",
				__func__, tsif_mode);
		}

		/* Set TSIF buffer configuration */
		ret = tsif_set_buf_config(tsif_driver->tsif_handler,
						threshold,
						DMX_TSIF_CHUNKS_IN_BUF);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: tsif_set_buf_config (%d, %d) failed\n",
				__func__, threshold,
				DMX_TSIF_CHUNKS_IN_BUF);
			MPQ_DVB_ERR_PRINT("Using default TSIF driver values\n");
		}

		/* Start TSIF driver */
		ret = tsif_start(tsif_driver->tsif_handler);
		if (ret < 0) {
			mutex_unlock(&mpq_dmx_tsif_info.tsif[tsif].mutex);
			MPQ_DVB_ERR_PRINT("%s: tsif_start failed\n", __func__);
			return ret;
		}

		/*
		 * Get data buffer information from TSIF driver
		 * (must be called after tsif_start)
		 */
		tsif_get_info(tsif_driver->tsif_handler,
				&(tsif_driver->data_buffer),
				&(tsif_driver->buffer_size));

		/* save pointer to the mpq_demux we are working on */
		mpq_dmx_tsif_info.tsif[tsif].mpq_demux = mpq_demux;
	}
	mpq_dmx_tsif_info.tsif[tsif].ref_count++;

	mutex_unlock(&mpq_dmx_tsif_info.tsif[tsif].mutex);

	return ret;
}


/**
 * Stop TSIF operation and detach from TSIF driver.
 *
 * @mpq_demux: the mpq_demux we are working on.
 *
 * Return	error code.
 */
static int mpq_tsif_dmx_stop(struct mpq_demux *mpq_demux)
{
	int tsif;
	struct tsif_driver_info *tsif_driver;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	/* determine the TSIF we are reading from */
	if (mpq_demux->source == DMX_SOURCE_FRONT0) {
		tsif = 0;
	} else if (mpq_demux->source == DMX_SOURCE_FRONT1) {
		tsif = 1;
	} else {
		/* invalid source */
		MPQ_DVB_ERR_PRINT(
			"%s: invalid input source (%d)\n",
			__func__,
			mpq_demux->source);

		return -EINVAL;
	}

	if (mutex_lock_interruptible(&mpq_dmx_tsif_info.tsif[tsif].mutex))
		return -ERESTARTSYS;

	mpq_dmx_tsif_info.tsif[tsif].ref_count--;

	if (mpq_dmx_tsif_info.tsif[tsif].ref_count == 0) {
		tsif_driver = &(mpq_dmx_tsif_info.tsif[tsif].tsif_driver);
		tsif_stop(tsif_driver->tsif_handler);
		tsif_detach(tsif_driver->tsif_handler);
		tsif_driver->tsif_handler = NULL;
		tsif_driver->data_buffer = NULL;
		tsif_driver->buffer_size = 0;
		atomic_set(&mpq_dmx_tsif_info.tsif[tsif].data_cnt, 0);
		mpq_dmx_tsif_info.tsif[tsif].mpq_demux = NULL;
	}

	mutex_unlock(&mpq_dmx_tsif_info.tsif[tsif].mutex);

	return 0;
}


/**
 * Start filtering according to feed parameter.
 *
 * @feed: the feed we are working on.
 *
 * Return	error code.
 */
static int mpq_tsif_dmx_start_filtering(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	MPQ_DVB_DBG_PRINT(
		"%s(%d) executed\n",
		__func__,
		feed->pid);

	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid mpq_demux handle\n",
			__func__);

		return -EINVAL;
	}

	if (mpq_demux->source < DMX_SOURCE_DVR0) {
		/* Source from TSIF, need to configure TSIF hardware */
		ret = mpq_tsif_dmx_start(mpq_demux);

		if (ret < 0) {
			MPQ_DVB_DBG_PRINT(
				"%s: mpq_tsif_dmx_start failed(%d)\n",
				__func__,
				ret);
			return ret;
		}
	}

	/* Always feed sections/PES starting from a new one and
	 * do not partial transfer data from older one
	 */
	feed->pusi_seen = 0;

	ret = mpq_dmx_init_mpq_feed(feed);
	if (ret < 0) {
		MPQ_DVB_DBG_PRINT(
			"%s: mpq_dmx_init_mpq_feed failed(%d)\n",
			__func__,
			ret);

		if (mpq_demux->source < DMX_SOURCE_DVR0)
			mpq_tsif_dmx_stop(mpq_demux);

		return ret;
	}

	return ret;
}


/**
 * Stop filtering according to feed parameter.
 *
 * @feed: the feed we are working on.
 *
 * Return	error code.
 */
static int mpq_tsif_dmx_stop_filtering(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	MPQ_DVB_DBG_PRINT(
		"%s(%d) executed\n",
		__func__,
		feed->pid);

	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid mpq_demux handle\n",
			__func__);

		return -EINVAL;
	}

	mpq_dmx_terminate_feed(feed);

	if (mpq_demux->source < DMX_SOURCE_DVR0) {
		/* Source from TSIF, need to configure TSIF hardware */
		ret = mpq_tsif_dmx_stop(mpq_demux);
	}

	return ret;
}


/**
 * TSIF demux plugin write-to-decoder function.
 *
 * @feed: The feed we are working on.
 * @buf: The data buffer to process.
 * @len: The data buffer length.
 *
 * Return	error code
 */
static int mpq_tsif_dmx_write_to_decoder(
					struct dvb_demux_feed *feed,
					const u8 *buf,
					size_t len)
{
	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	/*
	 * It is assumed that this function is called once for each
	 * TS packet of the relevant feed.
	 */
	if (len > TSIF_PKT_SIZE)
		MPQ_DVB_DBG_PRINT(
				"%s: warnning - len larger than one packet\n",
				__func__);

	if (dvb_dmx_is_video_feed(feed))
		return mpq_dmx_process_video_packet(feed, buf);

	if (dvb_dmx_is_pcr_feed(feed))
		return mpq_dmx_process_pcr_packet(feed, buf);

	return 0;
}

/**
 * Returns demux capabilities of TSIF plugin
 *
 * @demux: demux device
 * @caps: Returned capbabilities
 *
 * Return     error code
 */
static int mpq_tsif_dmx_get_caps(struct dmx_demux *demux,
				struct dmx_caps *caps)
{
	struct dvb_demux *dvb_demux = demux->priv;

	if ((dvb_demux == NULL) || (caps == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid parameters\n",
			__func__);

		return -EINVAL;
	}

	caps->caps = DMX_CAP_PULL_MODE | DMX_CAP_VIDEO_DECODER_DATA |
		DMX_CAP_TS_INSERTION | DMX_CAP_VIDEO_INDEXING;
	caps->num_decoders = MPQ_ADAPTER_MAX_NUM_OF_INTERFACES;
	caps->num_demux_devices = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
	caps->num_pid_filters = dvb_demux->feednum;
	caps->num_section_filters = dvb_demux->filternum;
	caps->num_section_filters_per_pid = dvb_demux->filternum;
	caps->section_filter_length = DMX_FILTER_SIZE;
	caps->num_demod_inputs = TSIF_COUNT;
	caps->num_memory_inputs = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
	caps->max_bitrate = 144;
	caps->demod_input_max_bitrate = 72;
	caps->memory_input_max_bitrate = 72;
	caps->num_cipher_ops = 0;

	/* TSIF reports 3 bytes STC at unit of 27MHz/256 */
	caps->max_stc = (u64)0xFFFFFF * 256;

	/* Buffer requirements */
	caps->section.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->section.max_buffer_num = 1;
	caps->section.max_size = 0xFFFFFFFF;
	caps->section.size_alignment = 0;
	caps->pes.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->pes.max_buffer_num = 1;
	caps->pes.max_size = 0xFFFFFFFF;
	caps->pes.size_alignment = 0;
	caps->recording_188_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->recording_188_tsp.max_buffer_num = 1;
	caps->recording_188_tsp.max_size = 0xFFFFFFFF;
	caps->recording_188_tsp.size_alignment = 0;
	caps->recording_192_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->recording_192_tsp.max_buffer_num = 1;
	caps->recording_192_tsp.max_size = 0xFFFFFFFF;
	caps->recording_192_tsp.size_alignment = 0;
	caps->playback_188_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->playback_188_tsp.max_buffer_num = 1;
	caps->playback_188_tsp.max_size = 0xFFFFFFFF;
	caps->playback_188_tsp.size_alignment = 0;
	caps->playback_192_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->playback_192_tsp.max_buffer_num = 1;
	caps->playback_192_tsp.max_size = 0xFFFFFFFF;
	caps->playback_192_tsp.size_alignment = 0;
	caps->decoder.flags =
		DMX_BUFFER_SECURED_IF_DECRYPTED	|
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT	|
		DMX_BUFFER_LINEAR_GROUP_SUPPORT;
	caps->decoder.max_buffer_num = DMX_MAX_DECODER_BUFFER_NUM;
	caps->decoder.max_size = 0xFFFFFFFF;
	caps->decoder.size_alignment = SZ_4K;

	return 0;
}

/**
 * Reads TSIF STC from TSPP
 *
 * @demux: demux device
 * @num: STC number. 0 for TSIF0 and 1 for TSIF1.
 * @stc: STC value
 * @base: divisor to get 90KHz value
 *
 * Return     error code
 */
static int mpq_tsif_dmx_get_stc(struct dmx_demux *demux, unsigned int num,
		u64 *stc, unsigned int *base)
{
	struct tsif_driver_info *tsif_driver;
	u32 tcr_counter;

	if (!demux || !stc || !base)
		return -EINVAL;

	if (num == 0)
		tsif_driver = &mpq_dmx_tsif_info.tsif[0].tsif_driver;
	else if (num == 1)
		tsif_driver = &mpq_dmx_tsif_info.tsif[1].tsif_driver;
	else
		return -EINVAL;

	if (!tsif_driver->tsif_handler)
		return -ENODEV;

	tsif_get_ref_clk_counter(tsif_driver->tsif_handler, &tcr_counter);

	*stc = ((u64)tcr_counter) * 256; /* conversion to 27MHz */
	*base = 300; /* divisor to get 90KHz clock from stc value */

	return 0;
}

/**
 * Initialize a single demux device.
 *
 * @mpq_adapter: MPQ DVB adapter
 * @mpq_demux: The demux device to initialize
 *
 * Return	error code
 */
static int mpq_tsif_dmx_init(
		struct dvb_adapter *mpq_adapter,
		struct mpq_demux *mpq_demux)
{
	int result;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	/* Set the kernel-demux object capabilities */
	mpq_demux->demux.dmx.capabilities =
		DMX_TS_FILTERING		|
		DMX_PES_FILTERING		|
		DMX_SECTION_FILTERING		|
		DMX_MEMORY_BASED_FILTERING	|
		DMX_CRC_CHECKING		|
		DMX_TS_DESCRAMBLING;

	/* Set dvb-demux "virtual" function pointers */
	mpq_demux->demux.priv = (void *)mpq_demux;
	mpq_demux->demux.filternum = DMX_TSIF_MAX_SECTION_FILTER_NUM;
	mpq_demux->demux.feednum = MPQ_MAX_DMX_FILES;
	mpq_demux->demux.start_feed = mpq_tsif_dmx_start_filtering;
	mpq_demux->demux.stop_feed = mpq_tsif_dmx_stop_filtering;
	mpq_demux->demux.write_to_decoder = mpq_tsif_dmx_write_to_decoder;
	mpq_demux->demux.decoder_fullness_init = mpq_dmx_decoder_fullness_init;
	mpq_demux->demux.decoder_fullness_wait = mpq_dmx_decoder_fullness_wait;
	mpq_demux->demux.decoder_fullness_abort =
		mpq_dmx_decoder_fullness_abort;
	mpq_demux->demux.decoder_buffer_status = mpq_dmx_decoder_buffer_status;
	mpq_demux->demux.reuse_decoder_buffer = mpq_dmx_reuse_decoder_buffer;
	mpq_demux->demux.set_secure_mode = NULL;
	mpq_demux->demux.oob_command = mpq_dmx_oob_command;
	mpq_demux->demux.convert_ts = mpq_dmx_convert_tts;

	/* Initialize dvb_demux object */
	result = dvb_dmx_init(&mpq_demux->demux);
	if (result < 0) {
		MPQ_DVB_ERR_PRINT("%s: dvb_dmx_init failed\n", __func__);
		goto init_failed;
	}

	/* Now initailize the dmx-dev object */
	mpq_demux->dmxdev.filternum = MPQ_MAX_DMX_FILES;
	mpq_demux->dmxdev.demux = &mpq_demux->demux.dmx;
	mpq_demux->dmxdev.capabilities = DMXDEV_CAP_DUPLEX;

	mpq_demux->dmxdev.demux->set_source = mpq_dmx_set_source;
	mpq_demux->dmxdev.demux->get_stc = mpq_tsif_dmx_get_stc;
	mpq_demux->dmxdev.demux->get_caps = mpq_tsif_dmx_get_caps;
	mpq_demux->dmxdev.demux->map_buffer = mpq_dmx_map_buffer;
	mpq_demux->dmxdev.demux->unmap_buffer = mpq_dmx_unmap_buffer;

	result = dvb_dmxdev_init(&mpq_demux->dmxdev, mpq_adapter);
	if (result < 0) {
		MPQ_DVB_ERR_PRINT("%s: dvb_dmxdev_init failed (errno=%d)\n",
						  __func__,
						  result);
		goto init_failed_dmx_release;
	}

	/* Extend dvb-demux debugfs with TSIF statistics. */
	mpq_dmx_init_debugfs_entries(mpq_demux);

	return 0;

init_failed_dmx_release:
	dvb_dmx_release(&mpq_demux->demux);
init_failed:
	return result;
}


/**
 * Module initialization function.
 *
 * Return	error code
 */
static int __init mpq_dmx_tsif_plugin_init(void)
{
	int i;
	int ret;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	/* check module parameters validity */
	if (threshold < 1) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid threshold parameter, using %d instead\n",
			__func__, DMX_TSIF_PACKETS_IN_CHUNK_DEF);
		threshold = DMX_TSIF_PACKETS_IN_CHUNK_DEF;
	}
	if ((tsif_mode < 1) || (tsif_mode > 3)) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid mode parameter, using %d instead\n",
			__func__, DMX_TSIF_DRIVER_MODE_DEF);
		tsif_mode = DMX_TSIF_DRIVER_MODE_DEF;
	}

	for (i = 0; i < TSIF_COUNT; i++) {
		snprintf(mpq_dmx_tsif_info.tsif[i].name,
				TSIF_NAME_LENGTH,
				"dmx_tsif%d",
				i);

		atomic_set(&mpq_dmx_tsif_info.tsif[i].data_cnt, 0);
		init_waitqueue_head(&mpq_dmx_tsif_info.tsif[i].wait_queue);
		mpq_dmx_tsif_info.tsif[i].thread =
			kthread_run(
				mpq_dmx_tsif_thread, (void *)i,
				mpq_dmx_tsif_info.tsif[i].name);

		if (IS_ERR(mpq_dmx_tsif_info.tsif[i].thread)) {
			int j;

			for (j = 0; j < i; j++) {
				kthread_stop(mpq_dmx_tsif_info.tsif[j].thread);
				mutex_destroy(&mpq_dmx_tsif_info.tsif[j].mutex);
			}

			MPQ_DVB_ERR_PRINT(
				"%s: kthread_run failed\n",
				__func__);

			return -ENOMEM;
		}

		mutex_init(&mpq_dmx_tsif_info.tsif[i].mutex);

		mpq_dmx_tsif_info.tsif[i].tsif_driver.tsif_handler = NULL;
		mpq_dmx_tsif_info.tsif[i].ref_count = 0;
	}

	ret = mpq_dmx_plugin_init(mpq_tsif_dmx_init);

	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_plugin_init failed (errno=%d)\n",
			__func__,
			ret);

		for (i = 0; i < TSIF_COUNT; i++) {
			kthread_stop(mpq_dmx_tsif_info.tsif[i].thread);
			mutex_destroy(&mpq_dmx_tsif_info.tsif[i].mutex);
		}
	}

	return ret;
}


/**
 * Module exit function.
 */
static void __exit mpq_dmx_tsif_plugin_exit(void)
{
	int i;
	struct tsif_driver_info *tsif_driver;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	for (i = 0; i < TSIF_COUNT; i++) {
		mutex_lock(&mpq_dmx_tsif_info.tsif[i].mutex);

		tsif_driver = &(mpq_dmx_tsif_info.tsif[i].tsif_driver);
		if (mpq_dmx_tsif_info.tsif[i].ref_count > 0) {
			mpq_dmx_tsif_info.tsif[i].ref_count = 0;
			if (tsif_driver->tsif_handler)
				tsif_stop(tsif_driver->tsif_handler);
		}

		/* Detach from TSIF driver to avoid further notifications. */
		if (tsif_driver->tsif_handler)
			tsif_detach(tsif_driver->tsif_handler);

		mutex_unlock(&mpq_dmx_tsif_info.tsif[i].mutex);
		kthread_stop(mpq_dmx_tsif_info.tsif[i].thread);
		mutex_destroy(&mpq_dmx_tsif_info.tsif[i].mutex);
	}

	mpq_dmx_plugin_exit();
}


module_init(mpq_dmx_tsif_plugin_init);
module_exit(mpq_dmx_tsif_plugin_exit);

MODULE_DESCRIPTION("Qualcomm demux TSIF HW Plugin");
MODULE_LICENSE("GPL v2");

