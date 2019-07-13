/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MPQ_ADAPTER_H
#define _MPQ_ADAPTER_H

#include <media/dvbdev.h>
#include <media/dvb_demux.h>
#include "mpq_stream_buffer.h"



/** IDs of interfaces holding stream-buffers */
enum mpq_adapter_stream_if {
	/** Interface holding stream-buffer for video0 stream */
	MPQ_ADAPTER_VIDEO0_STREAM_IF = 0,

	/** Interface holding stream-buffer for video1 stream */
	MPQ_ADAPTER_VIDEO1_STREAM_IF = 1,

	/** Interface holding stream-buffer for video2 stream */
	MPQ_ADAPTER_VIDEO2_STREAM_IF = 2,

	/** Interface holding stream-buffer for video3 stream */
	MPQ_ADAPTER_VIDEO3_STREAM_IF = 3,

	/** Interface holding stream-buffer for audio0 stream */
	MPQ_ADAPTER_AUDIO0_STREAM_IF = 4,

	/** Interface holding stream-buffer for audio1 stream */
	MPQ_ADAPTER_AUDIO1_STREAM_IF = 5,

	/** Interface holding stream-buffer for audio2 stream */
	MPQ_ADAPTER_AUDIO2_STREAM_IF = 6,

	/** Interface holding stream-buffer for audio3 stream */
	MPQ_ADAPTER_AUDIO3_STREAM_IF = 7,

	/** Maximum number of interfaces holding stream-buffers */
	MPQ_ADAPTER_MAX_NUM_OF_INTERFACES,
};

enum dmx_packet_type {
	DMX_PES_PACKET,
	DMX_FRAMING_INFO_PACKET,
	DMX_EOS_PACKET,
	DMX_MARKER_PACKET
};

struct dmx_pts_dts_info {
	/** Indication whether PTS exist */
	int pts_exist;

	/** Indication whether DTS exist */
	int dts_exist;

	/** PTS value associated with the PES data if any */
	u64 pts;

	/** DTS value associated with the PES data if any */
	u64 dts;
};

struct dmx_framing_packet_info {
	/** framing pattern type, one of DMX_IDX_* definitions */
	u64 pattern_type;

	/** PTS/DTS information */
	struct dmx_pts_dts_info pts_dts_info;

	/** STC value attached to first TS packet holding the pattern */
	u64 stc;

	/*
	 * Number of TS packets with Transport Error Indicator (TEI)
	 * found while constructing the frame.
	 */
	__u32 transport_error_indicator_counter;

	/* Number of continuity errors found while constructing the frame */
	__u32 continuity_error_counter;

	/*
	 * Number of dropped bytes due to insufficient buffer space,
	 * since last reported frame.
	 */
	__u32 ts_dropped_bytes;

	/* Total number of TS packets holding the frame */
	__u32 ts_packets_num;
};

struct dmx_pes_packet_info {
	/** PTS/DTS information */
	struct dmx_pts_dts_info pts_dts_info;

	/** STC value attached to first TS packet holding the PES */
	u64 stc;
};

struct dmx_marker_info {
	/* marker id */
	u64 id;
};

/** The meta-data used for video interface */
struct mpq_adapter_video_meta_data {
	/** meta-data packet type */
	enum dmx_packet_type packet_type;

	/** packet-type specific information */
	union {
		struct dmx_framing_packet_info framing;
		struct dmx_pes_packet_info pes;
		struct dmx_marker_info marker;
	} info;
} __packed;

/** The meta-data used for audio interface */
struct mpq_adapter_audio_meta_data {
	/** meta-data packet type */
	enum dmx_packet_type packet_type;

	/** packet-type specific information */
	union {
		struct dmx_pes_packet_info pes;
		struct dmx_marker_info marker;
	} info;
} __packed;

/** Callback function to notify on registrations of specific interfaces */
typedef void (*mpq_adapter_stream_if_callback)(
				enum mpq_adapter_stream_if interface_id,
				void *user_param);


/**
 * mpq_adapter_get - Returns pointer to Qualcomm Technologies Inc. DVB adapter
 *
 * Return     dvb adapter or NULL if not exist.
 */
struct dvb_adapter *mpq_adapter_get(void);


/**
 * mpq_adapter_register_stream_if - Register a stream interface.
 *
 * @interface_id: The interface id
 * @stream_buffer: The buffer used for the interface
 *
 * Return     error status
 *
 * Stream interface used to connect between two units in tunneling
 * mode using mpq_streambuffer implementation.
 * The producer of the interface should register the new interface,
 * consumer may get the interface using mpq_adapter_get_stream_if.
 *
 * Note that the function holds a pointer to this interface,
 * stream_buffer pointer assumed to be valid as long as interface
 * is active.
 */
int mpq_adapter_register_stream_if(
		enum mpq_adapter_stream_if interface_id,
		struct mpq_streambuffer *stream_buffer);


/**
 * mpq_adapter_unregister_stream_if - Un-register a stream interface.
 *
 * @interface_id: The interface id
 *
 * Return     error status
 */
int mpq_adapter_unregister_stream_if(
		enum mpq_adapter_stream_if interface_id);


/**
 * mpq_adapter_get_stream_if - Get buffer used for a stream interface.
 *
 * @interface_id: The interface id
 * @stream_buffer: The returned stream buffer
 *
 * Return     error status
 */
int mpq_adapter_get_stream_if(
		enum mpq_adapter_stream_if interface_id,
		struct mpq_streambuffer **stream_buffer);


/**
 * mpq_adapter_notify_stream_if - Register notification
 * to be triggered when a stream interface is registered.
 *
 * @interface_id: The interface id
 * @callback: The callback to be triggered when the interface is registered
 * @user_param: A parameter that is passed back to the callback function
 *				when triggered.
 *
 * Return     error status
 *
 * Producer may use this to register notification when desired
 * interface registered in the system and query its information
 * afterwards using mpq_adapter_get_stream_if.
 * To remove the callback, this function should be called with NULL
 * value in callback parameter.
 */
int mpq_adapter_notify_stream_if(
		enum mpq_adapter_stream_if interface_id,
		mpq_adapter_stream_if_callback callback,
		void *user_param);

#endif /* _MPQ_ADAPTER_H */
