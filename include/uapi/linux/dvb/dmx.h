/* SPDX-License-Identifier: LGPL-2.1+ WITH Linux-syscall-note */
/*
 * dmx.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
 *                    for convergence integrated media GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _UAPI_DVBDMX_H_
#define _UAPI_DVBDMX_H_

#include <linux/types.h>
#ifndef __KERNEL__
#include <time.h>
#endif


#define DMX_FILTER_SIZE 16

/* Min recording chunk upon which event is generated */
#define DMX_REC_BUFF_CHUNK_MIN_SIZE		(100*188)

#define DMX_MAX_DECODER_BUFFER_NUM		(32)

/**
 * enum dmx_output - Output for the demux.
 *
 * @DMX_OUT_DECODER:
 *	Streaming directly to decoder.
 * @DMX_OUT_TAP:
 *	Output going to a memory buffer (to be retrieved via the read command).
 *	Delivers the stream output to the demux device on which the ioctl
 *	is called.
 * @DMX_OUT_TS_TAP:
 *	Output multiplexed into a new TS (to be retrieved by reading from the
 *	logical DVR device). Routes output to the logical DVR device
 *	``/dev/dvb/adapter?/dvr?``, which delivers a TS multiplexed from all
 *	filters for which @DMX_OUT_TS_TAP was specified.
 * @DMX_OUT_TSDEMUX_TAP:
 *	Like @DMX_OUT_TS_TAP but retrieved from the DMX device.
 */
enum dmx_output {
	DMX_OUT_DECODER,
	DMX_OUT_TAP,
	DMX_OUT_TS_TAP,
	DMX_OUT_TSDEMUX_TAP
};


/**
 * enum dmx_input - Input from the demux.
 *
 * @DMX_IN_FRONTEND:	Input from a front-end device.
 * @DMX_IN_DVR:		Input from the logical DVR device.
 */
enum dmx_input {
	DMX_IN_FRONTEND,
	DMX_IN_DVR
};

/**
 * enum dmx_ts_pes - type of the PES filter.
 *
 * @DMX_PES_AUDIO0:	first audio PID. Also referred as @DMX_PES_AUDIO.
 * @DMX_PES_VIDEO0:	first video PID. Also referred as @DMX_PES_VIDEO.
 * @DMX_PES_TELETEXT0:	first teletext PID. Also referred as @DMX_PES_TELETEXT.
 * @DMX_PES_SUBTITLE0:	first subtitle PID. Also referred as @DMX_PES_SUBTITLE.
 * @DMX_PES_PCR0:	first Program Clock Reference PID.
 *			Also referred as @DMX_PES_PCR.
 *
 * @DMX_PES_AUDIO1:	second audio PID.
 * @DMX_PES_VIDEO1:	second video PID.
 * @DMX_PES_TELETEXT1:	second teletext PID.
 * @DMX_PES_SUBTITLE1:	second subtitle PID.
 * @DMX_PES_PCR1:	second Program Clock Reference PID.
 *
 * @DMX_PES_AUDIO2:	third audio PID.
 * @DMX_PES_VIDEO2:	third video PID.
 * @DMX_PES_TELETEXT2:	third teletext PID.
 * @DMX_PES_SUBTITLE2:	third subtitle PID.
 * @DMX_PES_PCR2:	third Program Clock Reference PID.
 *
 * @DMX_PES_AUDIO3:	fourth audio PID.
 * @DMX_PES_VIDEO3:	fourth video PID.
 * @DMX_PES_TELETEXT3:	fourth teletext PID.
 * @DMX_PES_SUBTITLE3:	fourth subtitle PID.
 * @DMX_PES_PCR3:	fourth Program Clock Reference PID.
 *
 * @DMX_PES_OTHER:	any other PID.
 */

enum dmx_ts_pes {
	DMX_PES_AUDIO0,
	DMX_PES_VIDEO0,
	DMX_PES_TELETEXT0,
	DMX_PES_SUBTITLE0,
	DMX_PES_PCR0,

	DMX_PES_AUDIO1,
	DMX_PES_VIDEO1,
	DMX_PES_TELETEXT1,
	DMX_PES_SUBTITLE1,
	DMX_PES_PCR1,

	DMX_PES_AUDIO2,
	DMX_PES_VIDEO2,
	DMX_PES_TELETEXT2,
	DMX_PES_SUBTITLE2,
	DMX_PES_PCR2,

	DMX_PES_AUDIO3,
	DMX_PES_VIDEO3,
	DMX_PES_TELETEXT3,
	DMX_PES_SUBTITLE3,
	DMX_PES_PCR3,

	DMX_PES_OTHER
};

#define DMX_PES_AUDIO    DMX_PES_AUDIO0
#define DMX_PES_VIDEO    DMX_PES_VIDEO0
#define DMX_PES_TELETEXT DMX_PES_TELETEXT0
#define DMX_PES_SUBTITLE DMX_PES_SUBTITLE0
#define DMX_PES_PCR      DMX_PES_PCR0



/**
 * struct dmx_filter - Specifies a section header filter.
 *
 * @filter: bit array with bits to be matched at the section header.
 * @mask: bits that are valid at the filter bit array.
 * @mode: mode of match: if bit is zero, it will match if equal (positive
 *	  match); if bit is one, it will match if the bit is negated.
 *
 * Note: All arrays in this struct have a size of DMX_FILTER_SIZE (16 bytes).
 */
struct dmx_filter {
	__u8  filter[DMX_FILTER_SIZE];
	__u8  mask[DMX_FILTER_SIZE];
	__u8  mode[DMX_FILTER_SIZE];
};

/**
 * struct dmx_sct_filter_params - Specifies a section filter.
 *
 * @pid: PID to be filtered.
 * @filter: section header filter, as defined by &struct dmx_filter.
 * @timeout: maximum time to filter, in milliseconds.
 * @flags: extra flags for the section filter.
 *
 * Carries the configuration for a MPEG-TS section filter.
 *
 * The @flags can be:
 *
 *	- %DMX_CHECK_CRC - only deliver sections where the CRC check succeeded;
 *	- %DMX_ONESHOT - disable the section filter after one section
 *	  has been delivered;
 *	- %DMX_IMMEDIATE_START - Start filter immediately without requiring a
 *	  :ref:`DMX_START`.
 */
struct dmx_sct_filter_params {
	__u16             pid;
	struct dmx_filter filter;
	__u32             timeout;
	__u32             flags;
#define DMX_CHECK_CRC       1
#define DMX_ONESHOT         2
#define DMX_IMMEDIATE_START 4
#define DMX_KERNEL_CLIENT   0x8000
};

enum dmx_video_codec {
	DMX_VIDEO_CODEC_MPEG2,
	DMX_VIDEO_CODEC_H264,
	DMX_VIDEO_CODEC_VC1
};

/* Index entries types */
#define DMX_IDX_RAI                         0x00000001
#define DMX_IDX_PUSI                        0x00000002
#define DMX_IDX_MPEG_SEQ_HEADER             0x00000004
#define DMX_IDX_MPEG_GOP                    0x00000008
#define DMX_IDX_MPEG_FIRST_SEQ_FRAME_START  0x00000010
#define DMX_IDX_MPEG_FIRST_SEQ_FRAME_END    0x00000020
#define DMX_IDX_MPEG_I_FRAME_START          0x00000040
#define DMX_IDX_MPEG_I_FRAME_END            0x00000080
#define DMX_IDX_MPEG_P_FRAME_START          0x00000100
#define DMX_IDX_MPEG_P_FRAME_END            0x00000200
#define DMX_IDX_MPEG_B_FRAME_START          0x00000400
#define DMX_IDX_MPEG_B_FRAME_END            0x00000800
#define DMX_IDX_H264_SPS                    0x00001000
#define DMX_IDX_H264_PPS                    0x00002000
#define DMX_IDX_H264_FIRST_SPS_FRAME_START  0x00004000
#define DMX_IDX_H264_FIRST_SPS_FRAME_END    0x00008000
#define DMX_IDX_H264_IDR_START              0x00010000
#define DMX_IDX_H264_IDR_END                0x00020000
#define DMX_IDX_H264_NON_IDR_START          0x00040000
#define DMX_IDX_H264_NON_IDR_END            0x00080000
#define DMX_IDX_VC1_SEQ_HEADER              0x00100000
#define DMX_IDX_VC1_ENTRY_POINT             0x00200000
#define DMX_IDX_VC1_FIRST_SEQ_FRAME_START   0x00400000
#define DMX_IDX_VC1_FIRST_SEQ_FRAME_END     0x00800000
#define DMX_IDX_VC1_FRAME_START             0x01000000
#define DMX_IDX_VC1_FRAME_END               0x02000000
#define DMX_IDX_H264_ACCESS_UNIT_DEL        0x04000000
#define DMX_IDX_H264_SEI                    0x08000000

/**
 * struct dmx_pes_filter_params - Specifies Packetized Elementary Stream (PES)
 *	filter parameters.
 *
 * @pid:	PID to be filtered.
 * @input:	Demux input, as specified by &enum dmx_input.
 * @output:	Demux output, as specified by &enum dmx_output.
 * @pes_type:	Type of the pes filter, as specified by &enum dmx_pes_type.
 * @flags:	Demux PES flags.
 */
struct dmx_pes_filter_params {
	__u16           pid;
	enum dmx_input  input;
	enum dmx_output output;
	enum dmx_ts_pes pes_type;
	__u32           flags;

	/*
	 * The following configures when the event
	 * DMX_EVENT_NEW_REC_CHUNK will be triggered.
	 * When new recorded data is received with size
	 * equal or larger than this value a new event
	 * will be triggered. This is relevant when
	 * output is DMX_OUT_TS_TAP or DMX_OUT_TSDEMUX_TAP,
	 * size must be at least DMX_REC_BUFF_CHUNK_MIN_SIZE
	 * and smaller than buffer size.
	 */
	__u32          rec_chunk_size;

	enum dmx_video_codec video_codec;
};

struct dmx_buffer_status {
	/* size of buffer in bytes */
	unsigned int size;

	/* fullness of buffer in bytes */
	unsigned int fullness;

	/*
	 * How many bytes are free
	 * It's the same as: size-fullness-1
	 */
	unsigned int free_bytes;

	/* read pointer offset in bytes */
	unsigned int read_offset;

	/* write pointer offset in bytes */
	unsigned int write_offset;

	/* non-zero if data error occurred */
	int error;
};

/* Events associated with each demux filter */
enum dmx_event {
	/* New PES packet is ready to be consumed */
	DMX_EVENT_NEW_PES = 0x00000001,

	/* New section is ready to be consumed */
	DMX_EVENT_NEW_SECTION = 0x00000002,

	/* New recording chunk is ready to be consumed */
	DMX_EVENT_NEW_REC_CHUNK = 0x00000004,

	/* New PCR value is ready */
	DMX_EVENT_NEW_PCR = 0x00000008,

	/* Overflow */
	DMX_EVENT_BUFFER_OVERFLOW = 0x00000010,

	/* Section was dropped due to CRC error */
	DMX_EVENT_SECTION_CRC_ERROR = 0x00000020,

	/* End-of-stream, no more data from this filter */
	DMX_EVENT_EOS = 0x00000040,

	/* New Elementary Stream data is ready */
	DMX_EVENT_NEW_ES_DATA = 0x00000080,

	/* Data markers */
	DMX_EVENT_MARKER = 0x00000100,

	/* New indexing entry is ready */
	DMX_EVENT_NEW_INDEX_ENTRY = 0x00000200,

	/*
	 * Section filter timer expired. This is notified
	 * when timeout is configured to section filter
	 * (dmx_sct_filter_params) and no sections were
	 * received for the given time.
	 */
	DMX_EVENT_SECTION_TIMEOUT = 0x00000400,

	/* Scrambling bits change between clear and scrambled */
	DMX_EVENT_SCRAMBLING_STATUS_CHANGE = 0x00000800
};

enum dmx_oob_cmd {
	/* End-of-stream, no more data from this filter */
	DMX_OOB_CMD_EOS,

	/* Data markers */
	DMX_OOB_CMD_MARKER,
};

/* Flags passed in filter events */

/* Continuity counter error was detected */
#define DMX_FILTER_CC_ERROR			0x01

/* Discontinuity indicator was set */
#define DMX_FILTER_DISCONTINUITY_INDICATOR	0x02

/* PES length in PES header is not correct */
#define DMX_FILTER_PES_LENGTH_ERROR		0x04


/* PES info associated with DMX_EVENT_NEW_PES event */
struct dmx_pes_event_info {
	/* Offset at which PES information starts */
	__u32 base_offset;

	/*
	 * Start offset at which PES data
	 * from the stream starts.
	 * Equal to base_offset if PES data
	 * starts from the beginning.
	 */
	__u32 start_offset;

	/* Total length holding the PES information */
	__u32 total_length;

	/* Actual length holding the PES data */
	__u32 actual_length;

	/* Local receiver timestamp in 27MHz */
	__u64 stc;

	/* Flags passed in filter events */
	__u32 flags;

	/*
	 * Number of TS packets with Transport Error Indicator (TEI)
	 * found while constructing the PES.
	 */
	__u32 transport_error_indicator_counter;

	/* Number of continuity errors found while constructing the PES */
	__u32 continuity_error_counter;

	/* Total number of TS packets holding the PES */
	__u32 ts_packets_num;
};

/* Section info associated with DMX_EVENT_NEW_SECTION event */
struct dmx_section_event_info {
	/* Offset at which section information starts */
	__u32 base_offset;

	/*
	 * Start offset at which section data
	 * from the stream starts.
	 * Equal to base_offset if section data
	 * starts from the beginning.
	 */
	__u32 start_offset;

	/* Total length holding the section information */
	__u32 total_length;

	/* Actual length holding the section data */
	__u32 actual_length;

	/* Flags passed in filter events */
	__u32 flags;
};

/* Recording info associated with DMX_EVENT_NEW_REC_CHUNK event */
struct dmx_rec_chunk_event_info {
	/* Offset at which recording chunk starts */
	__u32 offset;

	/* Size of recording chunk in bytes */
	__u32 size;
};

/* PCR info associated with DMX_EVENT_NEW_PCR event */
struct dmx_pcr_event_info {
	/* Local timestamp in 27MHz
	 * when PCR packet was received
	 */
	__u64 stc;

	/* PCR value in 27MHz */
	__u64 pcr;

	/* Flags passed in filter events */
	__u32 flags;
};

/*
 * Elementary stream data information associated
 * with DMX_EVENT_NEW_ES_DATA event
 */
struct dmx_es_data_event_info {
	/* Buffer user-space handle */
	int buf_handle;

	/*
	 * Cookie to provide when releasing the buffer
	 * using the DMX_RELEASE_DECODER_BUFFER ioctl command
	 */
	int cookie;

	/* Offset of data from the beginning of the buffer */
	__u32 offset;

	/* Length of data in buffer (in bytes) */
	__u32 data_len;

	/* Indication whether PTS value is valid */
	int pts_valid;

	/* PTS value associated with the buffer */
	__u64 pts;

	/* Indication whether DTS value is valid */
	int dts_valid;

	/* DTS value associated with the buffer */
	__u64 dts;

	/* STC value associated with the buffer in 27MHz */
	__u64 stc;

	/*
	 * Number of TS packets with Transport Error Indicator (TEI) set
	 * in the TS packet header since last reported event
	 */
	__u32 transport_error_indicator_counter;

	/* Number of continuity errors since last reported event */
	__u32 continuity_error_counter;

	/* Total number of TS packets processed since last reported event */
	__u32 ts_packets_num;

	/*
	 * Number of dropped bytes due to insufficient buffer space,
	 * since last reported event
	 */
	__u32 ts_dropped_bytes;
};

/* Marker details associated with DMX_EVENT_MARKER event */
struct dmx_marker_event_info {
	/* Marker id */
	__u64 id;
};

/* Indexing information associated with DMX_EVENT_NEW_INDEX_ENTRY event */
struct dmx_index_event_info {
	/* Index entry type, one of DMX_IDX_* */
	__u64 type;

	/*
	 * The PID the index entry belongs to.
	 * In case of recording filter, multiple PIDs may exist in the same
	 * filter through DMX_ADD_PID ioctl and each can be indexed separately.
	 */
	__u16 pid;

	/*
	 * The TS packet number in the recorded data at which
	 * the indexing event is found.
	 */
	__u64 match_tsp_num;

	/*
	 * The TS packet number in the recorded data preceding
	 * match_tsp_num and has PUSI set.
	 */
	__u64 last_pusi_tsp_num;

	/* STC associated with match_tsp_num, in 27MHz */
	__u64 stc;
};

/* Scrambling information associated with DMX_EVENT_SCRAMBLING_STATUS_CHANGE */
struct dmx_scrambling_status_event_info {
	/*
	 * The PID which its scrambling bit status changed.
	 * In case of recording filter, multiple PIDs may exist in the same
	 * filter through DMX_ADD_PID ioctl, each may have
	 * different scrambling bits status.
	 */
	__u16           pid;

	/* old value of scrambling bits */
	__u8 old_value;

	/* new value of scrambling bits */
	__u8 new_value;
};

/*
 * Filter's event returned through DMX_GET_EVENT.
 * poll with POLLPRI would block until events are available.
 */
struct dmx_filter_event {
	enum dmx_event type;

	union {
		struct dmx_pes_event_info pes;
		struct dmx_section_event_info section;
		struct dmx_rec_chunk_event_info recording_chunk;
		struct dmx_pcr_event_info pcr;
		struct dmx_es_data_event_info es_data;
		struct dmx_marker_event_info marker;
		struct dmx_index_event_info index;
		struct dmx_scrambling_status_event_info scrambling_status;
	} params;
};

/* Filter's buffer requirement returned in dmx_caps */
struct dmx_buffer_requirement {
	/* Buffer size alignment, 0 means no special requirement */
	__u32 size_alignment;

	/* Maximum buffer size allowed */
	__u32 max_size;

	/* Maximum number of linear buffers handled by demux */
	__u32 max_buffer_num;

	/* Feature support bitmap as detailed below */
	__u32           flags;

/* Buffer must be allocated as physically contiguous memory */
#define DMX_BUFFER_CONTIGUOUS_MEM		0x1

/* If the filter's data is decrypted, the buffer should be secured one */
#define DMX_BUFFER_SECURED_IF_DECRYPTED		0x2

/* Buffer can be allocated externally */
#define DMX_BUFFER_EXTERNAL_SUPPORT		0x4

/* Buffer can be allocated internally */
#define DMX_BUFFER_INTERNAL_SUPPORT		0x8

/* Filter output can be output to a linear buffer group */
#define DMX_BUFFER_LINEAR_GROUP_SUPPORT		0x10

/* Buffer may be allocated as cached buffer */
#define DMX_BUFFER_CACHED		0x20
};

/* Out-of-band (OOB) command */
struct dmx_oob_command {
	enum dmx_oob_cmd type;

	union {
		struct dmx_marker_event_info marker;
	} params;
};

struct dmx_caps {
	__u32 caps;

/* Indicates whether demux support playback from memory in pull mode */
#define DMX_CAP_PULL_MODE				0x01

/* Indicates whether demux support indexing of recorded video stream */
#define DMX_CAP_VIDEO_INDEXING			0x02

/* Indicates whether demux support sending data directly to video decoder */
#define DMX_CAP_VIDEO_DECODER_DATA		0x04

/* Indicates whether demux support sending data directly to audio decoder */
#define DMX_CAP_AUDIO_DECODER_DATA		0x08

/* Indicates whether demux support sending data directly to subtitle decoder */
#define DMX_CAP_SUBTITLE_DECODER_DATA	0x10

/* Indicates whether TS insertion is supported */
#define DMX_CAP_TS_INSERTION	0x20

/* Indicates whether playback from secured input is supported */
#define DMX_CAP_SECURED_INPUT_PLAYBACK	0x40

/* Indicates whether automatic buffer flush upon overflow is allowed */
#define DMX_CAP_AUTO_BUFFER_FLUSH	0x80

	/* Number of decoders demux can output data to */
	int num_decoders;

	/* Number of demux devices */
	int num_demux_devices;

	/* Max number of PID filters */
	int num_pid_filters;

	/* Max number of section filters */
	int num_section_filters;

	/*
	 * Max number of section filters using same PID,
	 * 0 if not supported
	 */
	int num_section_filters_per_pid;

	/*
	 * Length of section filter, not including section
	 * length field (2 bytes).
	 */
	int section_filter_length;

	/* Max number of demod based input */
	int num_demod_inputs;

	/* Max number of memory based input */
	int num_memory_inputs;

	/* Overall bitrate from all inputs concurrently. Mbit/sec */
	int max_bitrate;

	/* Max bitrate from single demod input. Mbit/sec */
	int demod_input_max_bitrate;

	/* Max bitrate from single memory input. Mbit/sec */
	int memory_input_max_bitrate;

	/* Max number of supported cipher operations per PID */
	int num_cipher_ops;

	/* Max possible value of STC reported by demux, in 27MHz */
	__u64 max_stc;

	/*
	 * For indexing support (DMX_CAP_VIDEO_INDEXING capability) this is
	 * the max number of video pids that can be indexed for a single
	 * recording filter. If 0, means there is not limitation.
	 */
	int recording_max_video_pids_indexed;

	struct dmx_buffer_requirement section;

	/* For PES not sent to decoder */
	struct dmx_buffer_requirement pes;

	/* For PES sent to decoder */
	struct dmx_buffer_requirement decoder;

	/* Recording buffer for recording of 188 bytes packets */
	struct dmx_buffer_requirement recording_188_tsp;

	/* Recording buffer for recording of 192 bytes packets */
	struct dmx_buffer_requirement recording_192_tsp;

	/* DVR input buffer for playback of 188 bytes packets */
	struct dmx_buffer_requirement playback_188_tsp;

	/* DVR input buffer for playback of 192 bytes packets */
	struct dmx_buffer_requirement playback_192_tsp;
};

enum dmx_source_t {
	DMX_SOURCE_FRONT0 = 0,
	DMX_SOURCE_FRONT1,
	DMX_SOURCE_FRONT2,
	DMX_SOURCE_FRONT3,
	DMX_SOURCE_DVR0   = 16,
	DMX_SOURCE_DVR1,
	DMX_SOURCE_DVR2,
	DMX_SOURCE_DVR3
};

enum dmx_tsp_format_t {
	DMX_TSP_FORMAT_188 = 0,
	DMX_TSP_FORMAT_192_TAIL,
	DMX_TSP_FORMAT_192_HEAD,
	DMX_TSP_FORMAT_204,
};

enum dmx_playback_mode_t {
	/*
	 * In push mode, if one of output buffers
	 * is full, the buffer would overflow
	 * and demux continue processing incoming stream.
	 * This is the default mode. When playing from frontend,
	 * this is the only mode that is allowed.
	 */
	DMX_PB_MODE_PUSH = 0,

	/*
	 * In pull mode, if one of output buffers
	 * is full, demux stalls waiting for free space,
	 * this would cause DVR input buffer fullness
	 * to accumulate.
	 * This mode is possible only when playing
	 * from DVR.
	 */
	DMX_PB_MODE_PULL,
};
/**
 * struct dmx_stc - Stores System Time Counter (STC) information.
 *
 * @num: input data: number of the STC, from 0 to N.
 * @base: output: divisor for STC to get 90 kHz clock.
 * @stc: output: stc in @base * 90 kHz units.
 */
struct dmx_stc {
	unsigned int num;
	unsigned int base;
	__u64 stc;
};

enum dmx_buffer_mode {
	/*
	 * demux buffers are allocated internally
	 * by the demux driver. This is the default mode.
	 * DMX_SET_BUFFER_SIZE can be used to set the size of
	 * this buffer.
	 */
	DMX_BUFFER_MODE_INTERNAL,

	/*
	 * demux buffers are allocated externally and provided
	 * to demux through DMX_SET_BUFFER.
	 * When this mode is used DMX_SET_BUFFER_SIZE and
	 * mmap are prohibited.
	 */
	DMX_BUFFER_MODE_EXTERNAL,
};

struct dmx_decoder_buffers {
	/*
	 * Specify if linear buffer support is requested. If set, buffers_num
	 * must be greater than 1
	 */
	int is_linear;

	/*
	 * Specify number of external buffers allocated by user.
	 * If set to 0 means internal buffer allocation is requested
	 */
	__u32 buffers_num;

	/* Specify buffer size, either external or internal */
	__u32 buffers_size;

	/* Array of externally allocated buffer handles */
	int handles[DMX_MAX_DECODER_BUFFER_NUM];
};

struct dmx_secure_mode {
	/*
	 * Specifies whether the filter is secure or not.
	 * Filter should be set as secured if the filter's data *may* include
	 * encrypted data that would require decryption configured through
	 * DMX_SET_CIPHER ioctl. The setting may be done while
	 * filter is in idle state only.
	 */
	int is_secured;
};

struct dmx_cipher_operation {
	/* Indication whether the operation is encryption or decryption */
	int encrypt;

	/* The ID of the key used for decryption or encryption */
	__u32 key_ladder_id;
};

#define DMX_MAX_CIPHER_OPERATIONS_COUNT	5
struct dmx_cipher_operations {
	/*
	 * The PID to perform the cipher operations on.
	 * In case of recording filter, multiple PIDs may exist in the same
	 * filter through DMX_ADD_PID ioctl, each may have different
	 * cipher operations.
	 */
	__u16 pid;

	/* Total number of operations */
	__u8 operations_count;

	/*
	 * Cipher operation to perform on the given PID.
	 * The operations are performed in the order they are given.
	 */
	struct dmx_cipher_operation operations[DMX_MAX_CIPHER_OPERATIONS_COUNT];
};

struct dmx_events_mask {
	/*
	 * Bitmask of events to be disabled (dmx_event).
	 * Disabled events will not be notified to the user.
	 * By default all events are enabled except for
	 * DMX_EVENT_NEW_ES_DATA.
	 * Overflow event can't be disabled.
	 */
	__u32 disable_mask;

	/*
	 * Bitmask of events that will not wake-up the user
	 * when user calls poll with POLLPRI flag.
	 * Events that are used as wake-up source should not be
	 * disabled in disable_mask or they would not be used
	 * as a wake-up source.
	 * By default all enabled events are set as wake-up events.
	 * Overflow event can't be disabled as a wake-up source.
	 */
	__u32 no_wakeup_mask;

	/*
	 * Number of ready wake-up events which will trigger
	 * a wake-up when user calls poll with POLLPRI flag.
	 * Default is set to 1.
	 */
	__u32 wakeup_threshold;
};

struct dmx_indexing_params {
	/*
	 * PID to index. In case of recording filter, multiple PIDs
	 * may exist in the same filter through DMX_ADD_PID ioctl.
	 * It is assumed that the PID was already added using DMX_ADD_PID
	 * or an error will be reported.
	 */
	__u16 pid;

	/* enable or disable indexing, default is disabled */
	int enable;

	/* combination of DMX_IDX_* bits */
	__u64 types;
};

struct dmx_set_ts_insertion {
	/*
	 * Unique identifier managed by the caller.
	 * This identifier can be used later to remove the
	 * insertion using DMX_ABORT_TS_INSERTION ioctl.
	 */
	__u32 identifier;

	/*
	 * Repetition time in msec, minimum allowed value is 25msec.
	 * 0 repetition time means one-shot insertion is done.
	 * Insertion done based on wall-clock.
	 */
	__u32 repetition_time;

	/*
	 * TS packets buffer to be inserted.
	 * The buffer is inserted as-is to the recording buffer
	 * without any modification.
	 * It is advised to set discontinuity flag in the very
	 * first TS packet in the buffer.
	 */
	const __u8 *ts_packets;

	/*
	 * Size in bytes of the TS packets buffer to be inserted.
	 * Should be in multiples of 188 or 192 bytes
	 * depending on recording filter output format.
	 */
	size_t size;
};

struct dmx_abort_ts_insertion {
	/*
	 * Identifier of the insertion buffer previously set
	 * using DMX_SET_TS_INSERTION.
	 */
	__u32 identifier;
};

struct dmx_scrambling_bits {
	/*
	 * The PID to return its scrambling bit value.
	 * In case of recording filter, multiple PIDs may exist in the same
	 * filter through DMX_ADD_PID ioctl, each may have different
	 * scrambling bits status.
	 */
	__u16 pid;

	/* Current value of scrambling bits: 0, 1, 2 or 3 */
	__u8 value;
};

/**
 * enum dmx_buffer_flags - DMX memory-mapped buffer flags
 *
 * @DMX_BUFFER_FLAG_HAD_CRC32_DISCARD:
 *	Indicates that the Kernel discarded one or more frames due to wrong
 *	CRC32 checksum.
 * @DMX_BUFFER_FLAG_TEI:
 *	Indicates that the Kernel has detected a Transport Error indicator
 *	(TEI) on a filtered pid.
 * @DMX_BUFFER_PKT_COUNTER_MISMATCH:
 *	Indicates that the Kernel has detected a packet counter mismatch
 *	on a filtered pid.
 * @DMX_BUFFER_FLAG_DISCONTINUITY_DETECTED:
 *	Indicates that the Kernel has detected one or more frame discontinuity.
 * @DMX_BUFFER_FLAG_DISCONTINUITY_INDICATOR:
 *	Received at least one packet with a frame discontinuity indicator.
 */

enum dmx_buffer_flags {
	DMX_BUF_FLAG_HAD_CRC32_DISCARD		= 1 << 0,
	DMX_BUF_FLAG_TEI			= 1 << 1,
	DMX_BUF_PKT_COUNTER_MISMATCH		= 1 << 2,
	DMX_BUF_FLAG_DISCONTINUITY_DETECTED	= 1 << 3,
	DMX_BUF_FLAG_DISCONTINUITY_INDICATOR	= 1 << 4,
};

/**
 * struct dmx_buffer - dmx buffer info
 *
 * @index:	id number of the buffer
 * @bytesused:	number of bytes occupied by data in the buffer (payload);
 * @offset:	for buffers with memory == DMX_MEMORY_MMAP;
 *		offset from the start of the device memory for this plane,
 *		(or a "cookie" that should be passed to mmap() as offset)
 * @length:	size in bytes of the buffer
 * @flags:	bit array of buffer flags as defined by &enum dmx_buffer_flags.
 *		Filled only at &DMX_DQBUF.
 * @count:	monotonic counter for filled buffers. Helps to identify
 *		data stream loses. Filled only at &DMX_DQBUF.
 *
 * Contains data exchanged by application and driver using one of the streaming
 * I/O methods.
 *
 * Please notice that, for &DMX_QBUF, only @index should be filled.
 * On &DMX_DQBUF calls, all fields will be filled by the Kernel.
 */
struct dmx_buffer {
	__u32			index;
	__u32			bytesused;
	__u32			offset;
	__u32			length;
	__u32			flags;
	__u32			count;
	unsigned int size;
	int handle;

	/*
	 * The following indication is relevant only when setting
	 * DVR input buffer. It indicates whether the input buffer
	 * being set is secured one or not. Secured (locked) buffers
	 * are required for playback from secured input. In such case
	 * write() syscall is not allowed.
	 */
	int is_protected;

};

/**
 * struct dmx_requestbuffers - request dmx buffer information
 *
 * @count:	number of requested buffers,
 * @size:	size in bytes of the requested buffer
 *
 * Contains data used for requesting a dmx buffer.
 * All reserved fields must be set to zero.
 */
struct dmx_requestbuffers {
	__u32			count;
	__u32			size;
};

/**
 * struct dmx_exportbuffer - export of dmx buffer as DMABUF file descriptor
 *
 * @index:	id number of the buffer
 * @flags:	flags for newly created file, currently only O_CLOEXEC is
 *		supported, refer to manual of open syscall for more details
 * @fd:		file descriptor associated with DMABUF (set by driver)
 *
 * Contains data used for exporting a dmx buffer as DMABUF file descriptor.
 * The buffer is identified by a 'cookie' returned by DMX_QUERYBUF
 * (identical to the cookie used to mmap() the buffer to userspace). All
 * reserved fields must be set to zero. The field reserved0 is expected to
 * become a structure 'type' allowing an alternative layout of the structure
 * content. Therefore this field should not be used for any other extensions.
 */
struct dmx_exportbuffer {
	__u32		index;
	__u32		flags;
	__s32		fd;
};

#define DMX_START                _IO('o', 41)
#define DMX_STOP                 _IO('o', 42)
#define DMX_SET_FILTER           _IOW('o', 43, struct dmx_sct_filter_params)
#define DMX_SET_PES_FILTER       _IOW('o', 44, struct dmx_pes_filter_params)
#define DMX_SET_BUFFER_SIZE      _IO('o', 45)
#define DMX_GET_PES_PIDS         _IOR('o', 47, __u16[5])
#define DMX_GET_CAPS             _IOR('o', 48, struct dmx_caps)
#define DMX_SET_SOURCE           _IOW('o', 49, enum dmx_source_t)
#define DMX_GET_STC              _IOWR('o', 50, struct dmx_stc)
#define DMX_ADD_PID              _IOW('o', 51, __u16)
#define DMX_REMOVE_PID           _IOW('o', 52, __u16)
#define DMX_SET_TS_PACKET_FORMAT _IOW('o', 53, enum dmx_tsp_format_t)
#define DMX_SET_TS_OUT_FORMAT	 _IOW('o', 54, enum dmx_tsp_format_t)
#define DMX_SET_DECODER_BUFFER_SIZE	_IO('o', 55)
#define DMX_GET_BUFFER_STATUS	 _IOR('o', 56, struct dmx_buffer_status)
#define DMX_RELEASE_DATA		 _IO('o', 57)
#define DMX_FEED_DATA			 _IO('o', 58)
#define DMX_SET_PLAYBACK_MODE	 _IOW('o', 59, enum dmx_playback_mode_t)
#define DMX_SET_SECURE_MODE	     _IOW('o', 65, struct dmx_secure_mode)
#define DMX_SET_EVENTS_MASK	     _IOW('o', 66, struct dmx_events_mask)
#define DMX_GET_EVENTS_MASK	     _IOR('o', 67, struct dmx_events_mask)
#define DMX_PUSH_OOB_COMMAND	 _IOW('o', 68, struct dmx_oob_command)
#define DMX_SET_INDEXING_PARAMS  _IOW('o', 69, struct dmx_indexing_params)
#define DMX_SET_TS_INSERTION     _IOW('o', 70, struct dmx_set_ts_insertion)
#define DMX_ABORT_TS_INSERTION   _IOW('o', 71, struct dmx_abort_ts_insertion)
#define DMX_GET_SCRAMBLING_BITS  _IOWR('o', 72, struct dmx_scrambling_bits)
#define DMX_SET_CIPHER           _IOW('o', 73, struct dmx_cipher_operations)
#define DMX_FLUSH_BUFFER         _IO('o', 74)
#define DMX_GET_EVENT		     _IOR('o', 75, struct dmx_filter_event)
#define DMX_SET_BUFFER_MODE	     _IOW('o', 76, enum dmx_buffer_mode)
#define DMX_SET_BUFFER		     _IOW('o', 77, struct dmx_buffer)
#define DMX_SET_DECODER_BUFFER	 _IOW('o', 78, struct dmx_decoder_buffers)
#define DMX_REUSE_DECODER_BUFFER _IO('o', 79)
#if !defined(__KERNEL__)

/* This is needed for legacy userspace support */
typedef enum dmx_output dmx_output_t;
typedef enum dmx_input dmx_input_t;
typedef enum dmx_ts_pes dmx_pes_type_t;
typedef struct dmx_filter dmx_filter_t;

#endif

#define DMX_REQBUFS              _IOWR('o', 60, struct dmx_requestbuffers)
#define DMX_QUERYBUF             _IOWR('o', 61, struct dmx_buffer)
#define DMX_EXPBUF               _IOWR('o', 62, struct dmx_exportbuffer)
#define DMX_QBUF                 _IOWR('o', 63, struct dmx_buffer)
#define DMX_DQBUF                _IOWR('o', 64, struct dmx_buffer)

#endif /* _DVBDMX_H_ */
