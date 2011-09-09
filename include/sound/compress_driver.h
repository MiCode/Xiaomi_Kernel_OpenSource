/*
 *  compress_driver.h - compress offload driver definations
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
#ifndef __COMPRESS_DRIVER_H
#define __COMPRESS_DRIVER_H

#include <sound/compress_offload.h>
#include <sound/asound.h>
#include <sound/pcm.h>

struct snd_compr_ops;

/**
 * struct snd_compr_runtime: runtime stream description
 * @state: stream state
 * @ops: pointer to DSP callbacks
 * @buffer: pointer to kernel buffer, valid only when not in mmap mode or
 *	DSP doesn't implement copy
 * @buffer_size: size of the above buffer
 * @fragment_size: size of buffer fragment in bytes
 * @fragments: number of such fragments
 * @hw_pointer: offset of last location in buffer where DSP copied data
 * @app_pointer: offset of last location in buffer where app wrote data
 * @sleep: poll sleep
 */
struct snd_compr_runtime {
	snd_pcm_state_t state;
	struct snd_compr_ops *ops;
	void *buffer;
	size_t buffer_size;
	size_t fragment_size;
	unsigned int fragments;
	size_t hw_pointer;
	size_t app_pointer;
	wait_queue_head_t sleep;
};

/**
 * struct snd_compr_stream: compressed stream
 * @name: device name
 * @ops: pointer to DSP callbacks
 * @runtime: pointer to runtime structure
 * @device: device pointer
 * @direction: stream direction, playback/recording
 * @private_data: pointer to DSP private data
 */
struct snd_compr_stream {
	const char *name;
	struct snd_compr_ops *ops;
	struct snd_compr_runtime *runtime;
	struct snd_compr *device;
	unsigned int direction;
	void *private_data;
};

/**
 * struct snd_compr_ops: compressed path DSP operations
 * @open: Open the compressed stream
 * This callback is mandatory and shall keep dsp ready to receive the stream
 * parameter
 * @free: Close the compressed stream, mandatory
 * @set_params: Sets the compressed stream parameters, mandatory
 * This can be called in during stream creation only to set codec params
 * and the stream properties
 * @get_params: retrieve the codec parameters, mandatory
 * @trigger: Trigger operations like start, pause, resume, drain, stop.
 * This callback is mandatory
 * @pointer: Retrieve current h/w pointer information. Mandatory
 * @copy: Copy the compressed data to/from userspace, Optional
 * Can't be implemented if DSP supports mmap
 * @mmap: DSP mmap method to mmap DSP memory
 * @ack: Ack for DSP when data is written to audio buffer, Optional
 * Not valid if copy is implemented
 * @get_caps: Retrieve DSP capabilities, mandatory
 * @get_codec_caps: Retrieve capabilities for a specific codec, mandatory
 */
struct snd_compr_ops {
	int (*open)(struct snd_compr_stream *stream);
	int (*free)(struct snd_compr_stream *stream);
	int (*set_params)(struct snd_compr_stream *stream,
			struct snd_compr_params *params);
	int (*get_params)(struct snd_compr_stream *stream,
			struct snd_compr_params *params);
	int (*trigger)(struct snd_compr_stream *stream, int cmd);
	int (*pointer)(struct snd_compr_stream *stream,
			struct snd_compr_tstamp *tstamp);
	int (*copy)(struct snd_compr_stream *stream, const char __user *buf,
		       size_t count);
	int (*mmap)(struct snd_compr_stream *stream,
			struct vm_area_struct *vma);
	int (*ack)(struct snd_compr_stream *stream);
	int (*get_caps) (struct snd_compr_stream *stream,
			struct snd_compr_caps *caps);
	int (*get_codec_caps) (struct snd_compr_stream *stream,
			struct snd_compr_codec_caps *codec);
};

/**
 * struct snd_compr: Compressed device
 * @name: DSP device name
 * @dev: Device pointer
 * @lock: device lock
 * @ops: pointer to DSP callbacks
 * @private_data: pointer to DSP pvt data
 */
struct snd_compr {
	const char *name;
	struct device *dev;
	struct mutex lock;
	struct snd_compr_ops *ops;
	struct list_head list;
	void *private_data;
};

/* compress device register APIs */
int snd_compress_register(struct snd_compr *device);
int snd_compress_deregister(struct snd_compr *device);

/* dsp driver callback apis
 * For playback: driver should call snd_compress_fragment_elapsed() to let the
 * framework know that a fragment has been consumed from the ring buffer
 * For recording: we may want to know when a frame is available or when
 * at least one frame is available for userspace, a different
 * snd_compress_frame_elapsed() callback should be used
 */
void snd_compr_fragment_elapsed(struct snd_compr_stream *stream);
void snd_compr_frame_elapsed(struct snd_compr_stream *stream);

#endif
