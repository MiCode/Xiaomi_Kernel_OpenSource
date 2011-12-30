/*
 *  compress_offload.h - compress offload header definations
 *
 *  Copyright (C) 2011 Intel Corporation
 *  Authors:	Vinod Koul <vinod.koul@linux.intel.com>
 *		Pierre-Louis Bossart <pierre-louis.bossart@linux.intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#ifndef __COMPRESS_OFFLOAD_H
#define __COMPRESS_OFFLOAD_H

#include <linux/types.h>
/**
 * struct snd_compressed_buffer:compressed buffer
 * @fragment_size: size of buffer fragment in bytes
 * @fragments: number of such fragments
 */
struct snd_compressed_buffer {
	size_t fragment_size;
	int fragments;
};

/* */
struct snd_compr_params {
	struct snd_compressed_buffer buffer;
	struct snd_codec codec;
};

/**
 * struct snd_compr_tstamp: timestamp descriptor
 * @copied_bytes: Number of bytes offset in ring buffer to DSP
 * @copied_total: Total number of bytes copied from ring buffer to DSP
 * @decoded: Frames decoded by DSP
 * @rendered: Frames rendered by DSP into a mixer or an audio output
 * @sampling_rate: sampling rate of audio
 */
struct snd_compr_tstamp {
	size_t copied_bytes;
	size_t copied_total;
	size_t decoded;
	size_t rendered;
	__u32 sampling_rate;
	uint64_t timestamp;
};

/**
 * struct snd_compr_avail: avail descriptor
 * @avail: Number of bytes available in ring buffer for writing/reading
 * @tstamp: timestamp infomation
 */
struct snd_compr_avail {
	size_t avail;
	struct snd_compr_tstamp tstamp;
};

/**
 * struct snd_compr_caps: caps descriptor
 * @codecs: pointer to array of codecs
 * @min_fragment_size: minimum fragment supported by DSP
 * @max_fragment_size: maximum fragment supported by DSP
 * @min_fragments: min fragments supported by DSP
 * @max_fragments: max fragments supported by DSP
 * @num_codecs: number of codecs supported
 * @reserved: reserved field
 */
struct snd_compr_caps {
	__u32 num_codecs;
	__u32 min_fragment_size;
	__u32 max_fragment_size;
	__u32 min_fragments;
	__u32 max_fragments;
	__u32 codecs[MAX_NUM_CODECS];
	__u32 reserved[11];
};

/**
 * struct snd_compr_codec_caps: query capability of codec
 * @codec: codec for which capability is queried
 * @num_descriptors: number of codec descriptors
 * @descriptor: array of codec capability descriptor
 */
struct snd_compr_codec_caps {
	__u32 codec;
	__u32 num_descriptors;
	struct snd_codec_desc descriptor[MAX_NUM_CODEC_DESCRIPTORS];
};

/**
 * compress path ioctl definitions
 * SNDRV_COMPRESS_GET_CAPS: Query capability of DSP
 * SNDRV_COMPRESS_GET_CODEC_CAPS: Query capability of a codec
 * SNDRV_COMPRESS_SET_PARAMS: Set codec and stream parameters
 * Note: only codec params can be changed runtime and stream params cant be
 * SNDRV_COMPRESS_GET_PARAMS: Query codec and stream params
 * SNDRV_COMPRESS_TSTAMP: get the current timestamp value
 * SNDRV_COMPRESS_AVAIL: get the current buffer avail value.
 * This also queries the tstamp properties
 * SNDRV_COMPRESS_PAUSE: Pause the running stream
 * SNDRV_COMPRESS_RESUME: resume a paused stream
 * SNDRV_COMPRESS_START: Start a stream
 * SNDRV_COMPRESS_STOP: stop a running stream, discarding ring buffer content
 * and the buffers currently with DSP
 * SNDRV_COMPRESS_DRAIN: Play till end of buffers and stop after that
 */
#define SNDRV_COMPRESS_GET_CAPS		_IOWR('C', 0x00, struct snd_compr_caps *)
#define SNDRV_COMPRESS_GET_CODEC_CAPS	_IOWR('C', 0x01, struct snd_compr_codec_caps *)
#define SNDRV_COMPRESS_SET_PARAMS	_IOW('C', 0x02, struct snd_compr_params *)
#define SNDRV_COMPRESS_GET_PARAMS	_IOR('C', 0x03, struct snd_compr_params *)
#define SNDRV_COMPRESS_TSTAMP		_IOR('C', 0x10, struct snd_compr_tstamp *)
#define SNDRV_COMPRESS_AVAIL		_IOR('C', 0x11, struct snd_compr_avail *)
#define SNDRV_COMPRESS_PAUSE		_IO('C', 0x20)
#define SNDRV_COMPRESS_RESUME		_IO('C', 0x21)
#define SNDRV_COMPRESS_START		_IO('C', 0x22)
#define SNDRV_COMPRESS_STOP		_IO('C', 0x23)
#define SNDRV_COMPRESS_DRAIN		_IO('C', 0x24)
/*
 * TODO
 * 1. add mmap support
 *
 */
#define SND_COMPR_TRIGGER_DRAIN 7 /*FIXME move this to pcm.h */
#endif
