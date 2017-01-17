/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include <linux/msm_ion.h>

#include "dvbdev.h"
#include "dmxdev.h"
#include "demux.h"
#include "dvb_demux.h"
#include "dvb_frontend.h"
#include "mpq_adapter.h"
#include "mpq_sdmx.h"

#define TS_PACKET_SYNC_BYTE	(0x47)
#define TS_PACKET_SIZE		(188)
#define TS_PACKET_HEADER_LENGTH (4)

/* Length of mandatory fields that must exist in header of video PES */
#define PES_MANDATORY_FIELDS_LEN			9

/*
 * 500 PES header packets in the meta-data buffer,
 * should be more than enough
 */
#define VIDEO_NUM_OF_PES_PACKETS			500

#define VIDEO_META_DATA_PACKET_SIZE	\
	(DVB_RINGBUFFER_PKTHDRSIZE +	\
		sizeof(struct mpq_streambuffer_packet_header) + \
		sizeof(struct mpq_adapter_video_meta_data))

#define VIDEO_META_DATA_BUFFER_SIZE	\
	(VIDEO_NUM_OF_PES_PACKETS * VIDEO_META_DATA_PACKET_SIZE)

#define AUDIO_NUM_OF_PES_PACKETS			100

#define AUDIO_META_DATA_PACKET_SIZE	\
	(DVB_RINGBUFFER_PKTHDRSIZE +	\
		sizeof(struct mpq_streambuffer_packet_header) + \
		sizeof(struct mpq_adapter_audio_meta_data))

#define AUDIO_META_DATA_BUFFER_SIZE	\
	(AUDIO_NUM_OF_PES_PACKETS * AUDIO_META_DATA_PACKET_SIZE)

/* Max number open() request can be done on demux device */
#define MPQ_MAX_DMX_FILES				128

/* TSIF alias name length */
#define TSIF_NAME_LENGTH				20

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

/**
 * mpq_decoder_buffers_desc - decoder buffer(s) management information.
 *
 * @desc: Array of buffer descriptors as they are passed to mpq_streambuffer
 * upon its initialization. These descriptors must remain valid as long as
 * the mpq_streambuffer object is used.
 * @ion_handle: Array of ION handles, one for each decoder buffer, used for
 * kernel memory mapping or allocation. Handles are saved in order to release
 * resources properly later on.
 * @decoder_buffers_num: number of buffers that are managed, either externally
 * or internally by the mpq_streambuffer object
 * @shared_file: File handle of internally allocated video buffer shared
 * with video consumer.
 */
struct mpq_decoder_buffers_desc {
	struct mpq_streambuffer_buffer_desc desc[DMX_MAX_DECODER_BUFFER_NUM];
	struct ion_handle *ion_handle[DMX_MAX_DECODER_BUFFER_NUM];
	u32 decoder_buffers_num;
	struct file *shared_file;
};

/*
 * mpq_video_feed_info - private data used for video feed.
 *
 * @video_buffer: Holds the streamer buffer shared with
 * the decoder for feeds having the data going to the decoder.
 * @video_buffer_lock: Lock protecting against video output buffer.
 * The lock protects against API calls to manipulate the output buffer
 * (initialize, free, re-use buffers) and dvb-sw demux parsing the video
 * data through mpq_dmx_process_video_packet().
 * @buffer_desc: Holds decoder buffer(s) information used for stream buffer.
 * @pes_header: Used for feeds that output data to decoder,
 * holds PES header of current processed PES.
 * @pes_header_left_bytes: Used for feeds that output data to decoder,
 * holds remaining PES header bytes of current processed PES.
 * @pes_header_offset: Holds the offset within the current processed
 * pes header.
 * @fullness_wait_cancel: Flag used to signal to abort waiting for
 * decoder's fullness.
 * @stream_interface: The ID of the video stream interface registered
 * with this stream buffer.
 * @patterns: pointer to the framing patterns to look for.
 * @patterns_num: number of framing patterns.
 * @prev_pattern: holds the trailing data of the last processed video packet.
 * @frame_offset: Saves data buffer offset to which a new frame will be written
 * @last_pattern_offset: Holds the previous pattern offset
 * @pending_pattern_len: Accumulated number of data bytes that will be
 * reported for this frame.
 * @last_framing_match_type: Used for saving the type of
 * the previous pattern match found in this video feed.
 * @last_framing_match_stc: Used for saving the STC attached to TS packet
 * of the previous pattern match found in this video feed.
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
 * @first_prefix_size: used to save the prefix size used to find the first
 * pattern written to the stream buffer.
 * @saved_pts_dts_info: used to save PTS/DTS information until it is written.
 * @new_pts_dts_info: used to store PTS/DTS information from current PES header.
 * @saved_info_used: indicates if saved PTS/DTS information was used.
 * @new_info_exists: indicates if new PTS/DTS information exists in
 * new_pts_dts_info that should be saved to saved_pts_dts_info.
 * @first_pts_dts_copy: a flag used to indicate if PTS/DTS information needs
 * to be copied from the currently parsed PES header to the saved_pts_dts_info.
 * @tei_errs: Transport stream Transport Error Indicator (TEI) counter.
 * @last_continuity: last continuity counter value found in TS packet header.
 * Initialized to -1.
 * @continuity_errs: Transport stream continuity error counter.
 * @ts_packets_num: TS packets counter.
 * @ts_dropped_bytes: counts the number of bytes dropped due to insufficient
 * buffer space.
 * @prev_stc: STC attached to the previous video TS packet
 */
struct mpq_video_feed_info {
	struct mpq_streambuffer *video_buffer;
	spinlock_t video_buffer_lock;
	struct mpq_decoder_buffers_desc buffer_desc;
	struct pes_packet_header pes_header;
	u32 pes_header_left_bytes;
	u32 pes_header_offset;
	int fullness_wait_cancel;
	enum mpq_adapter_stream_if stream_interface;
const struct dvb_dmx_video_patterns *patterns[DVB_DMX_MAX_SEARCH_PATTERN_NUM];
	int patterns_num;
	char prev_pattern[DVB_DMX_MAX_PATTERN_LEN];
	u32 frame_offset;
	u32 last_pattern_offset;
	u32 pending_pattern_len;
	u64 last_framing_match_type;
	u64 last_framing_match_stc;
	int found_sequence_header_pattern;
	struct dvb_dmx_video_prefix_size_masks prefix_size;
	u32 first_prefix_size;
	struct dmx_pts_dts_info saved_pts_dts_info;
	struct dmx_pts_dts_info new_pts_dts_info;
	int saved_info_used;
	int new_info_exists;
	int first_pts_dts_copy;
	u32 tei_errs;
	int last_continuity;
	u32 continuity_errs;
	u32 ts_packets_num;
	u32 ts_dropped_bytes;
	u64 prev_stc;
};

/* require a bare minimal mpq_audio_feed_info struct */
struct mpq_audio_feed_info {
	struct mpq_streambuffer *audio_buffer;
	spinlock_t audio_buffer_lock;
	struct mpq_decoder_buffers_desc buffer_desc;
	struct pes_packet_header pes_header;
	u32 pes_header_left_bytes;
	u32 pes_header_offset;
	int fullness_wait_cancel;
	enum mpq_adapter_stream_if stream_interface;
	u32 frame_offset; /* pes frame offset */
	struct dmx_pts_dts_info saved_pts_dts_info;
	struct dmx_pts_dts_info new_pts_dts_info;
	int saved_info_used;
	int new_info_exists;
	int first_pts_dts_copy;
	u32 tei_errs;
	int last_continuity;
	u32 continuity_errs;
	u32 ts_packets_num;
	u32 ts_dropped_bytes;
	u64 prev_stc;
};

/**
 * mpq feed object - mpq common plugin feed information
 *
 * @dvb_demux_feed: Back pointer to dvb demux level feed object
 * @mpq_demux: Pointer to common mpq demux object
 * @plugin_priv: Plugin specific private data
 * @sdmx_filter_handle: Secure demux filter handle. Recording feed may share
 * same filter handle
 * @secondary_feed: Specifies if this feed shares filter handle with
 * other feeds
 * @metadata_buf: Ring buffer object for managing the metadata buffer
 * @metadata_buf_handle: Allocation handle for the metadata buffer
 * @session_id: Counter that is incremented every time feed is initialized
 * through mpq_dmx_init_mpq_feed
 * @sdmx_buf: Ring buffer object for intermediate output data from the sdmx
 * @sdmx_buf_handle: Allocation handle for the sdmx intermediate data buffer
 * @video_info: Video feed specific information
 */
struct mpq_feed {
	struct dvb_demux_feed *dvb_demux_feed;
	struct mpq_demux *mpq_demux;
	void *plugin_priv;

	/* Secure demux related */
	int sdmx_filter_handle;
	int secondary_feed;
	enum sdmx_filter filter_type;
	struct dvb_ringbuffer metadata_buf;
	struct ion_handle *metadata_buf_handle;

	u8 session_id;
	struct dvb_ringbuffer sdmx_buf;
	struct ion_handle *sdmx_buf_handle;

	struct mpq_video_feed_info video_info;
	struct mpq_audio_feed_info audio_info;
};

/**
 * struct mpq_demux - mpq demux information
 * @idx: Instance index
 * @demux: The dvb_demux instance used by mpq_demux
 * @dmxdev: The dmxdev instance used by mpq_demux
 * @fe_memory: Handle of front-end memory source to mpq_demux
 * @source: The current source connected to the demux
 * @is_initialized: Indicates whether this demux device was
 *                  initialized or not.
 * @ion_client: ION demux client used to allocate memory from ION.
 * @mutex: Lock used to protect against private feed data
 * @feeds: mpq common feed object pool
 * @num_active_feeds: Number of active mpq feeds
 * @num_secure_feeds: Number of secure feeds (have a sdmx filter associated)
 * currently allocated.
 * Used before each call to sdmx_process() to build up to date state.
 * @sdmx_session_handle: Secure demux open session handle
 * @sdmx_filter_count: Number of active secure demux filters
 * @sdmx_eos: End-of-stream indication flag for current sdmx session
 * @sdmx_filters_state: Array holding buffers status for each secure
 * demux filter.
 * @decoder_alloc_flags: ION flags to be used when allocating internally
 * @plugin_priv: Underlying plugin's own private data
 * @mpq_dmx_plugin_release: Underlying plugin's release function
 * @hw_notification_interval: Notification interval in msec,
 * exposed in debugfs.
 * @hw_notification_min_interval: Minimum notification internal in msec,
 * exposed in debugfs.
 * @hw_notification_count: Notification count, exposed in debugfs.
 * @hw_notification_size: Notification size in bytes, exposed in debugfs.
 * @hw_notification_min_size: Minimum notification size in bytes,
 * exposed in debugfs.
 * @decoder_stat: Decoder output statistics, exposed in debug-fs.
 * @sdmx_process_count: Total number of times sdmx_process is called.
 * @sdmx_process_time_sum: Total time sdmx_process takes.
 * @sdmx_process_time_average: Average time sdmx_process takes.
 * @sdmx_process_time_max: Max time sdmx_process takes.
 * @sdmx_process_packets_sum: Total packets number sdmx_process handled.
 * @sdmx_process_packets_average: Average packets number sdmx_process handled.
 * @sdmx_process_packets_min: Minimum packets number sdmx_process handled.
 * @last_notification_time: Time of last HW notification.
 */
struct mpq_demux {
	int idx;
	struct dvb_demux demux;
	struct dmxdev dmxdev;
	struct dmx_frontend fe_memory;
	dmx_source_t source;
	int is_initialized;
	struct ion_client *ion_client;
	struct mutex mutex;
	struct mpq_feed feeds[MPQ_MAX_DMX_FILES];
	u32 num_active_feeds;
	u32 num_secure_feeds;
	int sdmx_session_handle;
	int sdmx_session_ref_count;
	int sdmx_filter_count;
	int sdmx_eos;
	struct {
		/* SDMX filters status */
		struct sdmx_filter_status status[MPQ_MAX_DMX_FILES];

		/* Index of the feed respective to SDMX filter */
		u8 mpq_feed_idx[MPQ_MAX_DMX_FILES];

		/*
		 * Snapshot of session_id of the feed
		 * when SDMX process was called. This is used
		 * to identify whether the feed has been
		 * restarted when processing SDMX results.
		 * May happen when demux is stalled in playback
		 * from memory with PULL mode.
		 */
		u8 session_id[MPQ_MAX_DMX_FILES];
	} sdmx_filters_state;

	unsigned int decoder_alloc_flags;

	/* HW plugin specific */
	void *plugin_priv;
	int (*mpq_dmx_plugin_release)(struct mpq_demux *mpq_demux);

	/* debug-fs */
	u32 hw_notification_interval;
	u32 hw_notification_min_interval;
	u32 hw_notification_count;
	u32 hw_notification_size;
	u32 hw_notification_min_size;

	struct {
		/*
		 * Accumulated number of bytes
		 * dropped due to decoder buffer fullness.
		 */
		u32 drop_count;

		/* Counter incremeneted for each video frame output by demux */
		u32 out_count;

		/*
		 * Sum of intervals (msec) holding the time
		 * between two successive video frames output.
		 */
		u32 out_interval_sum;

		/*
		 * Average interval (msec) between two
		 * successive video frames output.
		 */
		u32 out_interval_average;

		/*
		 * Max interval (msec) between two
		 * successive video frames output.
		 */
		u32 out_interval_max;

		/* Counter for number of decoder packets with TEI bit set */
		u32 ts_errors;

		/*
		 * Counter for number of decoder packets
		 * with continuity counter errors.
		 */
		u32 cc_errors;

		/* Time of last video frame output */
		struct timespec out_last_time;
	} decoder_stat[MPQ_ADAPTER_MAX_NUM_OF_INTERFACES];

	u32 sdmx_process_count;
	u32 sdmx_process_time_sum;
	u32 sdmx_process_time_average;
	u32 sdmx_process_time_max;
	u32 sdmx_process_packets_sum;
	u32 sdmx_process_packets_average;
	u32 sdmx_process_packets_min;
	enum sdmx_log_level sdmx_log_level;

	struct timespec last_notification_time;
	int ts_packet_timestamp_source;
};

/**
 * mpq_dmx_init - initialization and registration function of
 * single MPQ demux device
 *
 * @adapter: The adapter to register mpq_demux to
 * @mpq_demux: The mpq demux to initialize
 *
 * Every HW plug-in needs to provide implementation of such
 * function that will be called for each demux device on the
 * module initialization. The function mpq_demux_plugin_init
 * should be called during the HW plug-in module initialization.
 */
typedef int (*mpq_dmx_init)(struct dvb_adapter *mpq_adapter,
	struct mpq_demux *demux);

/**
 * mpq_demux_plugin_init - Initialize demux devices and register
 * them to the dvb adapter.
 *
 * @dmx_init_func: Pointer to the function to be used
 *  to initialize demux of the underlying HW plugin.
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
 * mpq_dmx_map_buffer - map user-space buffer into kernel space.
 *
 * @demux: The demux device.
 * @dmx_buffer: The demux buffer from user-space, assumes that
 * buffer handle is ION file-handle.
 * @priv_handle: Saves ION-handle of the buffer imported by this function.
 * @kernel_mem: Saves kernel mapped address of the buffer.
 *
 * Return     error code
 *
 * The function maps the buffer into kernel memory only if the buffer
 * was not allocated with secure flag, otherwise the returned kernel
 * memory address is set to NULL.
 */
int mpq_dmx_map_buffer(struct dmx_demux *demux, struct dmx_buffer *dmx_buffer,
		void **priv_handle, void **kernel_mem);

/**
 * mpq_dmx_unmap_buffer - unmap user-space buffer from kernel space memory.
 *
 * @demux: The demux device.
 * @priv_handle: ION-handle of the buffer returned from mpq_dmx_map_buffer.
 *
 * Return     error code
 *
 * The function unmaps the buffer from kernel memory only if the buffer
 * was not allocated with secure flag.
 */
int mpq_dmx_unmap_buffer(struct dmx_demux *demux, void *priv_handle);

/**
 * mpq_dmx_decoder_fullness_init - Initialize waiting
 * mechanism on decoder's buffer fullness.
 *
 * @feed: The decoder's feed
 *
 * Return     error code.
 */
int mpq_dmx_decoder_fullness_init(struct dvb_demux_feed *feed);

/**
 * mpq_dmx_decoder_fullness_wait - Checks whether decoder buffer
 * have free space as required, if not, wait for it.
 *
 * @feed: The decoder's feed
 * @required_space: the required free space to wait for
 *
 * Return     error code.
 */
int mpq_dmx_decoder_fullness_wait(struct dvb_demux_feed *feed,
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
int mpq_dmx_decoder_fullness_abort(struct dvb_demux_feed *feed);

/**
 * mpq_dmx_decoder_buffer_status - Returns the
 * status of the decoder's buffer.
 *
 * @feed: The decoder's feed
 * @dmx_buffer_status: Status of decoder's buffer
 *
 * Return     error code.
 */
int mpq_dmx_decoder_buffer_status(struct dvb_demux_feed *feed,
		struct dmx_buffer_status *dmx_buffer_status);

/**
 * mpq_dmx_reuse_decoder_buffer - release buffer passed to decoder for reuse
 * by the stream-buffer.
 *
 * @feed: The decoder's feed.
 * @cookie: stream-buffer handle of the buffer.
 *
 * Return     error code
 *
 * The function releases the buffer provided by the stream-buffer
 * connected to the decoder back to the stream-buffer for reuse.
 */
int mpq_dmx_reuse_decoder_buffer(struct dvb_demux_feed *feed, int cookie);

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
int mpq_dmx_process_video_packet(struct dvb_demux_feed *feed, const u8 *buf);

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
int mpq_dmx_process_pcr_packet(struct dvb_demux_feed *feed, const u8 *buf);

/**
 * mpq_dmx_extract_pcr_and_dci() - Extract the PCR field and discontinuity
 * indicator from a TS packet buffer.
 *
 * @buf: TS packet buffer
 * @pcr: returned PCR value
 * @dci: returned discontinuity indicator
 *
 * Returns 1 if PCR was extracted, 0 otherwise.
 */
int mpq_dmx_extract_pcr_and_dci(const u8 *buf, u64 *pcr, int *dci);

/**
 * mpq_dmx_init_debugfs_entries -
 * Extend dvb-demux debugfs with mpq related entries (HW statistics and secure
 * demux log level).
 *
 * @mpq_demux: The mpq_demux device to initialize.
 */
void mpq_dmx_init_debugfs_entries(struct mpq_demux *mpq_demux);

/**
 * mpq_dmx_update_hw_statistics -
 * Update dvb-demux debugfs with HW notification statistics.
 *
 * @mpq_demux: The mpq_demux device to update.
 */
void mpq_dmx_update_hw_statistics(struct mpq_demux *mpq_demux);

/**
 * mpq_dmx_set_cipher_ops - Handles setting of cipher operations
 *
 * @feed: The feed to set its cipher operations
 * @cipher_ops: Cipher operations to be set
 *
 * This common function handles only the case when working with
 * secure-demux. When working with secure demux a single decrypt cipher
 * operation is allowed.
 *
 * Return error code
 */
int mpq_dmx_set_cipher_ops(struct dvb_demux_feed *feed,
		struct dmx_cipher_operations *cipher_ops);

/**
 * mpq_dmx_convert_tts - Convert timestamp attached by HW to each TS
 * packet to 27MHz.
 *
 * @feed: The feed with TTS attached
 * @timestamp: Buffer holding the timestamp attached by the HW
 * @timestampIn27Mhz: Timestamp result in 27MHz
 *
 * Return error code
 */
void mpq_dmx_convert_tts(struct dvb_demux_feed *feed,
		const u8 timestamp[TIMESTAMP_LEN],
		u64 *timestampIn27Mhz);

/**
 * mpq_sdmx_open_session - Handle the details of opening a new secure demux
 * session for the specified mpq demux instance. Multiple calls to this
 * is allowed, reference counting is managed to open it only when needed.
 *
 * @mpq_demux: mpq demux instance
 *
 * Return error code
 */
int mpq_sdmx_open_session(struct mpq_demux *mpq_demux);

/**
 * mpq_sdmx_close_session - Closes secure demux session. The session
 * is closed only if reference counter of the session reaches 0.
 *
 * @mpq_demux: mpq demux instance
 *
 * Return error code
 */
int mpq_sdmx_close_session(struct mpq_demux *mpq_demux);

/**
 * mpq_dmx_init_mpq_feed - Initialize an mpq feed object
 * The function allocates mpq_feed object and saves in the dvb_demux_feed
 * priv field.
 *
 * @feed: A dvb demux level feed parent object
 *
 * Return error code
 */
int mpq_dmx_init_mpq_feed(struct dvb_demux_feed *feed);

/**
 * mpq_dmx_terminate_feed - Destroy an mpq feed object
 *
 * @feed: A dvb demux level feed parent object
 *
 * Return error code
 */
int mpq_dmx_terminate_feed(struct dvb_demux_feed *feed);

/**
 * mpq_dmx_init_video_feed() - Initializes video related data structures
 *
 * @mpq_feed:	mpq_feed object to initialize
 *
 * Return error code
 */
int mpq_dmx_init_video_feed(struct mpq_feed *mpq_feed);

/**
 * mpq_dmx_terminate_video_feed() - Release video related feed resources
 *
 * @mpq_feed:	mpq_feed object to terminate
 *
 * Return error code
 */
int mpq_dmx_terminate_video_feed(struct mpq_feed *mpq_feed);

/**
 * mpq_dmx_write - demux write() function implementation.
 *
 * A wrapper function used for writing new data into the demux via DVR.
 * It checks where new data should actually go, the secure demux or the normal
 * dvb demux software demux.
 *
 * @demux: demux interface
 * @buf: input buffer
 * @count: number of data bytes in input buffer
 *
 * Return number of bytes processed or error code
 */
int mpq_dmx_write(struct dmx_demux *demux, const char *buf, size_t count);

/**
 * mpq_sdmx_process - Perform demuxing process on the specified input buffer
 * in the secure demux instance
 *
 * @mpq_demux: mpq demux instance
 * @input: input buffer descriptor
 * @fill_count: number of data bytes in input buffer that can be read
 * @read_offset: offset in buffer for reading
 * @tsp_size: size of single TS packet
 *
 * Return number of bytes read or error code
 */
int mpq_sdmx_process(struct mpq_demux *mpq_demux,
	struct sdmx_buff_descr *input,
	u32 fill_count,
	u32 read_offset,
	size_t tsp_size);

/**
 * mpq_sdmx_loaded - Returns 1 if secure demux application is loaded,
 * 0 otherwise. This function should be used to determine whether or not
 * processing should take place in the SDMX.
 */
int mpq_sdmx_is_loaded(void);

/**
 * mpq_dmx_oob_command - Handles OOB command from dvb-demux.
 *
 * OOB marker commands trigger callback to the dmxdev.
 * Handling of EOS command may trigger current (last on stream) PES/Frame to
 * be reported, in addition to callback to the dmxdev.
 * In case secure demux is active for the feed, EOS command is passed to the
 * secure demux for handling.
 *
 * @feed: dvb demux feed object
 * @cmd: oob command data
 *
 * returns 0 on success or error
 */
int mpq_dmx_oob_command(struct dvb_demux_feed *feed,
	struct dmx_oob_command *cmd);

/**
 * mpq_dmx_peer_rec_feed() - For a recording filter with multiple feeds objects
 * search for a feed object that shares the same filter as the specified feed
 * object, and return it.
 * This can be used to test whether the specified feed object is the first feed
 * allocate for the recording filter - return value is NULL.
 *
 * @feed: dvb demux feed object
 *
 * Return the dvb_demux_feed sharing the same filter's buffer or NULL if no
 * such is found.
 */
struct dvb_demux_feed *mpq_dmx_peer_rec_feed(struct dvb_demux_feed *feed);

/**
 * mpq_dmx_decoder_eos_cmd() - Report EOS event to the mpq_streambuffer
 *
 * @mpq_feed: Audio/Video mpq_feed object for notification
 * @feed_type: Feed type( Audio or Video )
 *
 * Return error code
 */
int mpq_dmx_decoder_eos_cmd(struct mpq_feed *mpq_feed, int feed_type);

/**
 * mpq_dmx_parse_mandatory_pes_header() - Parse non-optional PES header fields
 * from TS packet buffer and save results in the feed object.
 *
 * @feed:		Video dvb demux feed object
 * @feed_data:		Structure where results will be saved
 * @pes_header:		Saved PES header
 * @buf:		Input buffer containing TS packet with the PES header
 * @ts_payload_offset:	Offset in 'buf' where payload begins
 * @bytes_avail:	Length of actual payload
 *
 * Return error code
 */
int mpq_dmx_parse_mandatory_pes_header(
				struct dvb_demux_feed *feed,
				struct mpq_video_feed_info *feed_data,
				struct pes_packet_header *pes_header,
				const u8 *buf,
				u32 *ts_payload_offset,
				int *bytes_avail);

/**
 * mpq_dmx_parse_remaining_pes_header() - Parse optional PES header fields
 * from TS packet buffer and save results in the feed object.
 * This function depends on mpq_dmx_parse_mandatory_pes_header being called
 * first for state to be valid.
 *
 * @feed:		Video dvb demux feed object
 * @feed_data:		Structure where results will be saved
 * @pes_header:		Saved PES header
 * @buf:		Input buffer containing TS packet with the PES header
 * @ts_payload_offset:	Offset in 'buf' where payload begins
 * @bytes_avail:	Length of actual payload
 *
 * Return error code
 */
int mpq_dmx_parse_remaining_pes_header(
				struct dvb_demux_feed *feed,
				struct mpq_video_feed_info *feed_data,
				struct pes_packet_header *pes_header,
				const u8 *buf,
				u32 *ts_payload_offset,
				int *bytes_avail);

/**
 * mpq_dmx_flush_stream_buffer() - Flush video stream buffer object of the
 * specific video feed, both meta-data packets and data.
 *
 * @feed:	dvb demux video feed object
 *
 * Return error code
 */
int mpq_dmx_flush_stream_buffer(struct dvb_demux_feed *feed);

/**
 * mpq_dmx_save_pts_dts() - Save the current PTS/DTS data
 *
 * @feed_data: Video feed structure where PTS/DTS is saved
 */
static inline void mpq_dmx_save_pts_dts(struct mpq_video_feed_info *feed_data)
{
	if (feed_data->new_info_exists) {
		feed_data->saved_pts_dts_info.pts_exist =
			feed_data->new_pts_dts_info.pts_exist;
		feed_data->saved_pts_dts_info.pts =
			feed_data->new_pts_dts_info.pts;
		feed_data->saved_pts_dts_info.dts_exist =
			feed_data->new_pts_dts_info.dts_exist;
		feed_data->saved_pts_dts_info.dts =
			feed_data->new_pts_dts_info.dts;

		feed_data->new_info_exists = 0;
		feed_data->saved_info_used = 0;
	}
}

/**
 * mpq_dmx_write_pts_dts() - Write out the saved PTS/DTS data and mark as used
 *
 * @feed_data:	Video feed structure where PTS/DTS was saved
 * @info:	PTS/DTS structure to write to
 */
static inline void mpq_dmx_write_pts_dts(struct mpq_video_feed_info *feed_data,
					struct dmx_pts_dts_info *info)
{
	if (!feed_data->saved_info_used) {
		info->pts_exist = feed_data->saved_pts_dts_info.pts_exist;
		info->pts = feed_data->saved_pts_dts_info.pts;
		info->dts_exist = feed_data->saved_pts_dts_info.dts_exist;
		info->dts = feed_data->saved_pts_dts_info.dts;

		feed_data->saved_info_used = 1;
	} else {
		info->pts_exist = 0;
		info->dts_exist = 0;
	}
}

/*
 * mpq_dmx_calc_time_delta -
 * Calculate delta in msec between two time snapshots.
 *
 * @curr_time: value of current time
 * @prev_time: value of previous time
 *
 * Return	time-delta in msec
 */
static inline u32 mpq_dmx_calc_time_delta(struct timespec *curr_time,
	struct timespec *prev_time)
{
	struct timespec delta_time;
	u64 delta_time_ms;

	delta_time = timespec_sub(*curr_time, *prev_time);

	delta_time_ms = ((u64)delta_time.tv_sec * MSEC_PER_SEC) +
		delta_time.tv_nsec / NSEC_PER_MSEC;

	return (u32)delta_time_ms;
}

void mpq_dmx_update_decoder_stat(struct mpq_feed *mpq_feed);

/* Return the common module parameter tsif_mode */
int mpq_dmx_get_param_tsif_mode(void);

/* Return the common module parameter clock_inv */
int mpq_dmx_get_param_clock_inv(void);

/* Return the common module parameter mpq_sdmx_scramble_odd */
int mpq_dmx_get_param_scramble_odd(void);

/* Return the common module parameter mpq_sdmx_scramble_even */
int mpq_dmx_get_param_scramble_even(void);

/* Return the common module parameter mpq_sdmx_scramble_default_discard */
int mpq_dmx_get_param_scramble_default_discard(void);

/* APIs for Audio stream buffers interface -- Added for broadcase use case */
/*
 * The Audio/Video drivers (or consumers) require the stream_buffer information
 * for consuming packet headers and compressed AV data from the
 * ring buffer filled by demux driver which is the producer
 */
struct mpq_streambuffer *consumer_audio_streambuffer(int dmx_ts_pes_audio);
struct mpq_streambuffer *consumer_video_streambuffer(int dmx_ts_pes_video);

int mpq_dmx_init_audio_feed(struct mpq_feed *mpq_feed);

int mpq_dmx_terminate_audio_feed(struct mpq_feed *mpq_feed);

int mpq_dmx_parse_remaining_audio_pes_header(
				struct dvb_demux_feed *feed,
				struct mpq_audio_feed_info *feed_data,
				struct pes_packet_header *pes_header,
				const u8 *buf,
				u32 *ts_payload_offset,
				int *bytes_avail);

static inline void mpq_dmx_save_audio_pts_dts(
				struct mpq_audio_feed_info *feed_data)
{
	if (feed_data->new_info_exists) {
		feed_data->saved_pts_dts_info.pts_exist =
			feed_data->new_pts_dts_info.pts_exist;
		feed_data->saved_pts_dts_info.pts =
			feed_data->new_pts_dts_info.pts;
		feed_data->saved_pts_dts_info.dts_exist =
			feed_data->new_pts_dts_info.dts_exist;
		feed_data->saved_pts_dts_info.dts =
			feed_data->new_pts_dts_info.dts;

		feed_data->new_info_exists = 0;
		feed_data->saved_info_used = 0;
	}
}

/*
 * mpq_dmx_process_audio_packet - Assemble Audio PES data and output to
 * stream buffer connected to decoder.
 */
int mpq_dmx_process_audio_packet(struct dvb_demux_feed *feed, const u8 *buf);

static inline void mpq_dmx_write_audio_pts_dts(
					struct mpq_audio_feed_info *feed_data,
					struct dmx_pts_dts_info *info)
{
	if (!feed_data->saved_info_used) {
		info->pts_exist = feed_data->saved_pts_dts_info.pts_exist;
		info->pts = feed_data->saved_pts_dts_info.pts;
		info->dts_exist = feed_data->saved_pts_dts_info.dts_exist;
		info->dts = feed_data->saved_pts_dts_info.dts;

		feed_data->saved_info_used = 1;
	} else {
		info->pts_exist = 0;
		info->dts_exist = 0;
	}
}

#endif /* _MPQ_DMX_PLUGIN_COMMON_H */
