/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef _MPQ_DMX_PLUGIN_COMMON_H
#define _MPQ_DMX_PLUGIN_COMMON_H

#include <linux/ion.h>

#include "dvbdev.h"
#include "dmxdev.h"
#include "demux.h"
#include "dvb_demux.h"
#include "dvb_frontend.h"
#include "mpq_adapter.h"


/* Max number open() request can be done on demux device */
#define MPQ_MAX_DMX_FILES				128


/**
 * TSIF alias name length
 */
#define TSIF_NAME_LENGTH				10

#define MPQ_MAX_FOUND_PATTERNS				5

/**
 * struct mpq_demux - mpq demux information
 * @demux: The dvb_demux instance used by mpq_demux
 * @dmxdev: The dmxdev instance used by mpq_demux
 * @fe_memory: Handle of front-end memory source to mpq_demux
 * @source: The current source connected to the demux
 * @is_initialized: Indicates whether this demux device was
 *                  initialized or not.
 * @ion_client: ION demux client used to allocate memory from ION.
 * @feed_lock: Lock used to protect against private feed data
 * @hw_notification_rate: Notification rate in msec, exposed in debugfs.
 * @hw_notification_count: Notification count, exposed in debugfs.
 * @hw_notification_size: Notification size in bytes, exposed in debugfs.
 * @decoder_tsp_drop_count: Counter of number of dropped TS packets
 * due to decoder buffer fullness, exposed in debugfs.
 * @last_notification_time: Time of last HW notification.
 */
struct mpq_demux {
	struct dvb_demux demux;
	struct dmxdev dmxdev;
	struct dmx_frontend fe_memory;
	dmx_source_t source;
	int is_initialized;
	struct ion_client *ion_client;
	spinlock_t feed_lock;

	/* debug-fs */
	u32 hw_notification_rate;
	u32 hw_notification_count;
	u32 hw_notification_size;
	u32 decoder_tsp_drop_count;
	struct timespec last_notification_time;
};

/**
 * mpq_dmx_init - initialization and registration function of
 * single MPQ demux device
 *
 * @adapter: The adapter to register mpq_demux to
 * @mpq_demux: The mpq demux to initialize
 *
 * Every HW pluging need to provide implementation of such
 * function that will be called for each demux device on the
 * module initialization. The function mpq_demux_plugin_init
 * should be called during the HW plugin module initialization.
 */
typedef int (*mpq_dmx_init)(
				struct dvb_adapter *mpq_adapter,
				struct mpq_demux *demux);

/**
 * struct ts_packet_header - Transport packet header
 * as defined in MPEG2 transport stream standard.
 */
struct ts_packet_header {
#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned sync_byte:8;
	unsigned transport_error_indicator:1;
	unsigned payload_unit_start_indicator:1;
	unsigned transport_priority:1;
	unsigned pid_msb:5;
	unsigned pid_lsb:8;
	unsigned transport_scrambling_control:2;
	unsigned adaptation_field_control:2;
	unsigned continuity_counter:4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned sync_byte:8;
	unsigned pid_msb:5;
	unsigned transport_priority:1;
	unsigned payload_unit_start_indicator:1;
	unsigned transport_error_indicator:1;
	unsigned pid_lsb:8;
	unsigned continuity_counter:4;
	unsigned adaptation_field_control:2;
	unsigned transport_scrambling_control:2;
#else
#error "Please fix <asm/byteorder.h>"
#endif
} __packed;

/**
 * struct ts_adaptation_field - Adaptation field prefix
 * as defined in MPEG2 transport stream standard.
 */
struct ts_adaptation_field {
#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned adaptation_field_length:8;
	unsigned discontinuity_indicator:1;
	unsigned random_access_indicator:1;
	unsigned elementary_stream_priority_indicator:1;
	unsigned PCR_flag:1;
	unsigned OPCR_flag:1;
	unsigned splicing_point_flag:1;
	unsigned transport_private_data_flag:1;
	unsigned adaptation_field_extension_flag:1;
	unsigned program_clock_reference_base_1:8;
	unsigned program_clock_reference_base_2:8;
	unsigned program_clock_reference_base_3:8;
	unsigned program_clock_reference_base_4:8;
	unsigned program_clock_reference_base_5:1;
	unsigned reserved:6;
	unsigned program_clock_reference_ext_1:1;
	unsigned program_clock_reference_ext_2:8;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned adaptation_field_length:8;
	unsigned adaptation_field_extension_flag:1;
	unsigned transport_private_data_flag:1;
	unsigned splicing_point_flag:1;
	unsigned OPCR_flag:1;
	unsigned PCR_flag:1;
	unsigned elementary_stream_priority_indicator:1;
	unsigned random_access_indicator:1;
	unsigned discontinuity_indicator:1;
	unsigned program_clock_reference_base_1:8;
	unsigned program_clock_reference_base_2:8;
	unsigned program_clock_reference_base_3:8;
	unsigned program_clock_reference_base_4:8;
	unsigned program_clock_reference_ext_1:1;
	unsigned reserved:6;
	unsigned program_clock_reference_base_5:1;
	unsigned program_clock_reference_ext_2:8;
#else
#error "Please fix <asm/byteorder.h>"
#endif
} __packed;


/*
 * PES packet header containing dts and/or pts values
 * as defined in MPEG2 transport stream standard.
 */
struct pes_packet_header {
#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned packet_start_code_prefix_1:8;
	unsigned packet_start_code_prefix_2:8;
	unsigned packet_start_code_prefix_3:8;
	unsigned stream_id:8;
	unsigned pes_packet_length_msb:8;
	unsigned pes_packet_length_lsb:8;
	unsigned reserved_bits0:2;
	unsigned pes_scrambling_control:2;
	unsigned pes_priority:1;
	unsigned data_alignment_indicator:1;
	unsigned copyright:1;
	unsigned original_or_copy:1;
	unsigned pts_dts_flag:2;
	unsigned escr_flag:1;
	unsigned es_rate_flag:1;
	unsigned dsm_trick_mode_flag:1;
	unsigned additional_copy_info_flag:1;
	unsigned pes_crc_flag:1;
	unsigned pes_extension_flag:1;
	unsigned pes_header_data_length:8;
	unsigned reserved_bits1:4;
	unsigned pts_1:3;
	unsigned marker_bit0:1;
	unsigned pts_2:8;
	unsigned pts_3:7;
	unsigned marker_bit1:1;
	unsigned pts_4:8;
	unsigned pts_5:7;
	unsigned marker_bit2:1;
	unsigned reserved_bits2:4;
	unsigned dts_1:3;
	unsigned marker_bit3:1;
	unsigned dts_2:8;
	unsigned dts_3:7;
	unsigned marker_bit4:1;
	unsigned dts_4:8;
	unsigned dts_5:7;
	unsigned marker_bit5:1;
	unsigned reserved_bits3:4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned packet_start_code_prefix_1:8;
	unsigned packet_start_code_prefix_2:8;
	unsigned packet_start_code_prefix_3:8;
	unsigned stream_id:8;
	unsigned pes_packet_length_lsb:8;
	unsigned pes_packet_length_msb:8;
	unsigned original_or_copy:1;
	unsigned copyright:1;
	unsigned data_alignment_indicator:1;
	unsigned pes_priority:1;
	unsigned pes_scrambling_control:2;
	unsigned reserved_bits0:2;
	unsigned pes_extension_flag:1;
	unsigned pes_crc_flag:1;
	unsigned additional_copy_info_flag:1;
	unsigned dsm_trick_mode_flag:1;
	unsigned es_rate_flag:1;
	unsigned escr_flag:1;
	unsigned pts_dts_flag:2;
	unsigned pes_header_data_length:8;
	unsigned marker_bit0:1;
	unsigned pts_1:3;
	unsigned reserved_bits1:4;
	unsigned pts_2:8;
	unsigned marker_bit1:1;
	unsigned pts_3:7;
	unsigned pts_4:8;
	unsigned marker_bit2:1;
	unsigned pts_5:7;
	unsigned marker_bit3:1;
	unsigned dts_1:3;
	unsigned reserved_bits2:4;
	unsigned dts_2:8;
	unsigned marker_bit4:1;
	unsigned dts_3:7;
	unsigned dts_4:8;
	unsigned marker_bit5:1;
	unsigned dts_5:7;
	unsigned reserved_bits3:4;
#else
#error "Please fix <asm/byteorder.h>"
#endif
} __packed;

/*
 * mpq_framing_prefix_size_masks - possible prefix sizes.
 *
 * @size_mask: a bit mask (per pattern) of possible prefix sizes to use
 * when searching for a pattern that started in the last buffer.
 * Updated in mpq_dmx_framing_pattern_search for use in the next lookup
 */
struct mpq_framing_prefix_size_masks {
	u32 size_mask[MPQ_MAX_FOUND_PATTERNS];
};

/*
 * mpq_video_feed_info - private data used for video feed.
 *
 * @plugin_data: Underlying plugin's own private data.
 * @video_buffer: Holds the streamer buffer shared with
 * the decoder for feeds having the data going to the decoder.
 * @pes_header: Used for feeds that output data to decoder,
 * holds PES header of current processed PES.
 * @pes_header_left_bytes: Used for feeds that output data to decoder,
 * holds remainning PES header bytes of current processed PES.
 * @pes_header_offset: Holds the offset within the current processed
 * pes header.
 * @fullness_wait_cancel: Flag used to signal to abort waiting for
 * decoder's fullness.
 * @pes_payload_address: Used for feeds that output data to decoder,
 * holds current PES payload start address.
 * @payload_buff_handle: ION handle for the allocated payload buffer
 * @stream_interface: The ID of the video stream interface registered
 * with this stream buffer.
 * @patterns: pointer to the framing patterns to look for.
 * @patterns_num: number of framing patterns.
 * @last_framing_match_address: Used for saving the raw data address of
 * the previous pattern match found in this video feed.
 * @last_framing_match_type: Used for saving the type of
 * the previous pattern match found in this video feed.
 * @found_sequence_header_pattern: Flag used to note that an MPEG-2
 * Sequence Header, H.264 SPS or VC-1 Sequence Header pattern
 * (whichever is relevant according to the video standard) had already
 * been found.
 * @prefix_size: a bit mask representing the size(s) of possible prefixes
 * to the pattern, already found in the previous buffer. If bit 0 is set,
 * a prefix of size 1 was found. If bit 1 is set, a prefix of size 2 was
 * found, etc. This supports a prefix size of up to 32, which is more
 * than we need. The search function updates prefix_size as needed
 * for the next buffer search.
 * @first_pattern_offset: used to save the offset of the first pattern written
 * to the stream buffer.
 * @first_prefix_size: used to save the prefix size used to find the first
 * pattern written to the stream buffer.
 * @write_pts_dts: Flag used to decide if to write PTS/DTS information
 * (if it is available in the PES header) in the meta-data passed
 * to the video decoder. PTS/DTS information is written in the first
 * packet after it is available.
 */
struct mpq_video_feed_info {
	void *plugin_data;
	struct mpq_streambuffer *video_buffer;
	struct pes_packet_header pes_header;
	u32 pes_header_left_bytes;
	u32 pes_header_offset;
	u32 pes_payload_address;
	int fullness_wait_cancel;
	struct ion_handle *payload_buff_handle;
	enum mpq_adapter_stream_if stream_interface;
	const struct mpq_framing_pattern_lookup_params *patterns;
	int patterns_num;
	u32 last_framing_match_address;
	enum dmx_framing_pattern_type last_framing_match_type;
	int found_sequence_header_pattern;
	struct mpq_framing_prefix_size_masks prefix_size;
	u32 first_pattern_offset;
	u32 first_prefix_size;
	int write_pts_dts;
};

/**
 * mpq_demux_plugin_init - Initialize demux devices and register
 * them to the dvb adapter.
 *
 * @dmx_init_func: Pointer to the function to be used
 *  to initialize demux of the udnerlying HW plugin.
 *
 * Return     error code
 *
 * Should be called at the HW plugin module initialization.
 */
int mpq_dmx_plugin_init(mpq_dmx_init dmx_init_func);

/**
 * mpq_demux_plugin_exit - terminate demux devices.
 *
 * Should be called at the HW plugin module termination.
 */
void mpq_dmx_plugin_exit(void);

/**
 * mpq_dmx_set_source - implmenetation of set_source routine.
 *
 * @demux: The demux device to set its source.
 * @src: The source to be set.
 *
 * Return     error code
 *
 * Can be used by the underlying plugins to implement kernel
 * demux API set_source routine.
 */
int mpq_dmx_set_source(struct dmx_demux *demux, const dmx_source_t *src);

/**
 * mpq_dmx_init_video_feed - Initializes video feed
 * used to pass data to decoder directly.
 *
 * @feed: The feed used for the video TS packets
 *
 * Return     error code.
 *
 * If the underlying plugin wishes to perform SW PES assmebly
 * for the video data and stream it to the decoder, it should
 * call this function when video feed is initialized before
 * using mpq_dmx_process_video_packet.
 *
 * The function allocates mpq_video_feed_info and saves in
 * feed->priv.
 */
int mpq_dmx_init_video_feed(struct dvb_demux_feed *feed);

/**
 * mpq_dmx_terminate_video_feed - Free private data of
 * video feed allocated in mpq_dmx_init_video_feed
 *
 * @feed: The feed used for the video TS packets
 *
 * Return     error code.
 */
int mpq_dmx_terminate_video_feed(struct dvb_demux_feed *feed);

/**
 * mpq_dmx_decoder_fullness_init - Initialize waiting
 * mechanism on decoder's buffer fullness.
 *
 * @feed: The decoder's feed
 *
 * Return     error code.
 */
int mpq_dmx_decoder_fullness_init(
		struct dvb_demux_feed *feed);

/**
 * mpq_dmx_decoder_fullness_wait - Checks whether decoder buffer
 * have free space as required, if not, wait for it.
 *
 * @feed: The decoder's feed
 * @required_space: the required free space to wait for
 *
 * Return     error code.
 */
int mpq_dmx_decoder_fullness_wait(
		struct dvb_demux_feed *feed,
		size_t required_space);

/**
 * mpq_dmx_decoder_fullness_abort - Aborts waiting
 * on decoder's buffer fullness if any waiting is done
 * now. After calling this, to wait again the user must
 * call mpq_dmx_decoder_fullness_init.
 *
 * @feed: The decoder's feed
 *
 * Return     error code.
 */
int mpq_dmx_decoder_fullness_abort(
		struct dvb_demux_feed *feed);

/**
 * mpq_dmx_process_video_packet - Assemble PES data and output it
 * to the stream-buffer connected to the decoder.
 *
 * @feed: The feed used for the video TS packets
 * @buf: The buffer holding video TS packet.
 *
 * Return     error code.
 *
 * The function assumes it receives buffer with single TS packet
 * of the relevant PID.
 * If the output buffer is full while assembly, the function drops
 * the packet and does not write them to the output buffer.
 * Scrambled packets are bypassed.
 */
int mpq_dmx_process_video_packet(
		struct dvb_demux_feed *feed,
		const u8 *buf);

/**
 * mpq_dmx_process_pcr_packet - Extract PCR/STC pairs from
 * a 192 bytes packet.
 *
 * @feed: The feed used for the PCR TS packets
 * @buf: The buffer holding pcr/stc packet.
 *
 * Return     error code.
 *
 * The function assumes it receives buffer with single TS packet
 * of the relevant PID, and that it has 4 bytes
 * suffix as extra timestamp in the following format:
 *
 * Byte3: TSIF flags
 * Byte0-2: TTS, 0..2^24-1 at 105.47 Khz (27*10^6/256).
 *
 * The function callbacks dmxdev after extraction of the pcr/stc
 * pair.
 */
int mpq_dmx_process_pcr_packet(
			struct dvb_demux_feed *feed,
			const u8 *buf);

/**
 * mpq_dmx_is_video_feed - Returns whether the PES feed
 * is video one.
 *
 * @feed: The feed to be checked.
 *
 * Return     1 if feed is video feed, 0 otherwise.
 */
static inline int mpq_dmx_is_video_feed(struct dvb_demux_feed *feed)
{
	if (feed->type != DMX_TYPE_TS)
		return 0;

	if (feed->ts_type & (~TS_DECODER))
		return 0;

	if ((feed->pes_type == DMX_TS_PES_VIDEO0) ||
		(feed->pes_type == DMX_TS_PES_VIDEO1) ||
		(feed->pes_type == DMX_TS_PES_VIDEO2) ||
		(feed->pes_type == DMX_TS_PES_VIDEO3))
		return 1;

	return 0;
}

/**
 * mpq_dmx_is_pcr_feed - Returns whether the PES feed
 * is PCR one.
 *
 * @feed: The feed to be checked.
 *
 * Return     1 if feed is PCR feed, 0 otherwise.
 */
static inline int mpq_dmx_is_pcr_feed(struct dvb_demux_feed *feed)
{
	if (feed->type != DMX_TYPE_TS)
		return 0;

	if (feed->ts_type & (~TS_DECODER))
		return 0;

	if ((feed->pes_type == DMX_TS_PES_PCR0) ||
		(feed->pes_type == DMX_TS_PES_PCR1) ||
		(feed->pes_type == DMX_TS_PES_PCR2) ||
		(feed->pes_type == DMX_TS_PES_PCR3))
		return 1;

	return 0;
}

/**
 * mpq_dmx_init_hw_statistics -
 * Extend dvb-demux debugfs with HW statistics.
 *
 * @mpq_demux: The mpq_demux device to initialize.
 */
void mpq_dmx_init_hw_statistics(struct mpq_demux *mpq_demux);


/**
 * mpq_dmx_update_hw_statistics -
 * Update dvb-demux debugfs with HW notification statistics.
 *
 * @mpq_demux: The mpq_demux device to update.
 */
void mpq_dmx_update_hw_statistics(struct mpq_demux *mpq_demux);

#endif /* _MPQ_DMX_PLUGIN_COMMON_H */

