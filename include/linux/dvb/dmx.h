/*
 * dmx.h
 *
 * Copyright (C) 2000 Marcus Metzler <marcus@convergence.de>
 *                  & Ralph  Metzler <ralph@convergence.de>
 *                    for convergence integrated media GmbH
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef _DVBDMX_H_
#define _DVBDMX_H_

#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/time.h>
#else
#include <time.h>
#endif


#define DMX_FILTER_SIZE 16

typedef enum
{
	DMX_OUT_DECODER, /* Streaming directly to decoder. */
	DMX_OUT_TAP,     /* Output going to a memory buffer */
			 /* (to be retrieved via the read command).*/
	DMX_OUT_TS_TAP,  /* Output multiplexed into a new TS  */
			 /* (to be retrieved by reading from the */
			 /* logical DVR device).                 */
	DMX_OUT_TSDEMUX_TAP /* Like TS_TAP but retrieved from the DMX device */
} dmx_output_t;


typedef enum
{
	DMX_IN_FRONTEND, /* Input from a front-end device.  */
	DMX_IN_DVR       /* Input from the logical DVR device.  */
} dmx_input_t;


typedef enum
{
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
} dmx_pes_type_t;

#define DMX_PES_AUDIO    DMX_PES_AUDIO0
#define DMX_PES_VIDEO    DMX_PES_VIDEO0
#define DMX_PES_TELETEXT DMX_PES_TELETEXT0
#define DMX_PES_SUBTITLE DMX_PES_SUBTITLE0
#define DMX_PES_PCR      DMX_PES_PCR0


typedef struct dmx_filter
{
	__u8  filter[DMX_FILTER_SIZE];
	__u8  mask[DMX_FILTER_SIZE];
	__u8  mode[DMX_FILTER_SIZE];
} dmx_filter_t;


/* Filter flags */
#define DMX_CHECK_CRC		0x01
#define DMX_ONESHOT		0x02
#define DMX_IMMEDIATE_START	0x04
#define DMX_ENABLE_INDEXING	0x08
#define DMX_KERNEL_CLIENT	0x8000

struct dmx_sct_filter_params
{
	__u16          pid;
	dmx_filter_t   filter;
	__u32          timeout;
	__u32          flags;
};


/* Indexing: supported video standards */
enum dmx_indexing_video_standard {
	DMX_INDEXING_MPEG2,
	DMX_INDEXING_H264,
	DMX_INDEXING_VC1
};

/* Indexing: Supported video profiles */
enum dmx_indexing_video_profile {
	DMX_INDEXING_MPEG2_ANY,
	DMX_INDEXING_H264_ANY,
	DMX_INDEXING_VC1_ANY
};

/* Indexing: video configuration parameters */
struct dmx_indexing_video_params {
	enum dmx_indexing_video_standard standard;
	enum dmx_indexing_video_profile profile;
};


struct dmx_pes_filter_params
{
	__u16          pid;
	dmx_input_t    input;
	dmx_output_t   output;
	dmx_pes_type_t pes_type;
	__u32          flags;

	struct dmx_indexing_video_params video_params;
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

	/* non-zero if data error occured */
	int error;
};

typedef struct dmx_caps {
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
} dmx_caps_t;

typedef enum {
	DMX_SOURCE_FRONT0 = 0,
	DMX_SOURCE_FRONT1,
	DMX_SOURCE_FRONT2,
	DMX_SOURCE_FRONT3,
	DMX_SOURCE_DVR0   = 16,
	DMX_SOURCE_DVR1,
	DMX_SOURCE_DVR2,
	DMX_SOURCE_DVR3
} dmx_source_t;

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

struct dmx_stc {
	unsigned int num;	/* input : which STC? 0..N */
	unsigned int base;	/* output: divisor for stc to get 90 kHz clock */
	__u64 stc;		/* output: stc in 'base'*90 kHz units */
};


#define DMX_START                _IO('o', 41)
#define DMX_STOP                 _IO('o', 42)
#define DMX_SET_FILTER           _IOW('o', 43, struct dmx_sct_filter_params)
#define DMX_SET_PES_FILTER       _IOW('o', 44, struct dmx_pes_filter_params)
#define DMX_SET_BUFFER_SIZE      _IO('o', 45)
#define DMX_GET_PES_PIDS         _IOR('o', 47, __u16[5])
#define DMX_GET_CAPS             _IOR('o', 48, dmx_caps_t)
#define DMX_SET_SOURCE           _IOW('o', 49, dmx_source_t)
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

#endif /*_DVBDMX_H_*/
