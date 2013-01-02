/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/vmalloc.h>
#include <linux/file.h>
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"


/* Length of mandatory fields that must exist in header of video PES */
#define PES_MANDATORY_FIELDS_LEN			9


/*
 * 500 PES header packets in the meta-data buffer,
 * should be more than enough
 */
#define VIDEO_NUM_OF_PES_PACKETS			500

#define VIDEO_META_DATA_BUFFER_SIZE              \
	(VIDEO_NUM_OF_PES_PACKETS *                  \
	  (DVB_RINGBUFFER_PKTHDRSIZE +               \
	   sizeof(struct mpq_streambuffer_packet_header) + \
	   sizeof(struct mpq_adapter_video_meta_data)))


/* Number of demux devices, has default of linux configuration */
static int mpq_demux_device_num = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
module_param(mpq_demux_device_num, int, S_IRUGO);

/* ION heap IDs used for allocating video output buffer */
static int video_secure_ion_heap = ION_CP_MM_HEAP_ID;
module_param(video_secure_ion_heap , int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(video_secure_ion_heap, "ION heap for secure video buffer allocation");

static int video_nonsecure_ion_heap = ION_IOMMU_HEAP_ID;
module_param(video_nonsecure_ion_heap, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(video_nonsecure_ion_heap, "ION heap for non-secure video buffer allocation");

static int generate_es_events;
module_param(generate_es_events, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(generate_es_events, "Generate new elementary stream data events");

/**
 * Maximum allowed framing pattern size
 */
#define MPQ_MAX_PATTERN_SIZE				6

/**
 * Number of patterns to look for when doing framing, per video standard
 */
#define MPQ_MPEG2_PATTERN_NUM				5
#define MPQ_H264_PATTERN_NUM				5
#define MPQ_VC1_PATTERN_NUM				3

/*
 * mpq_framing_pattern_lookup_params - framing pattern lookup parameters.
 *
 * @pattern: the byte pattern to look for.
 * @mask: the byte mask to use (same length as pattern).
 * @size: the length of the pattern, in bytes.
 * @type: the type of the pattern.
 */
struct mpq_framing_pattern_lookup_params {
	u8 pattern[MPQ_MAX_PATTERN_SIZE];
	u8 mask[MPQ_MAX_PATTERN_SIZE];
	size_t size;
	enum dmx_framing_pattern_type type;
};

/*
 * Pre-defined video framing lookup pattern information.
 * Note: the first pattern in each patterns database must
 * be the Sequence Header (or equivalent SPS in H.264).
 * The code assumes this is the case when prepending
 * Sequence Header data in case it is required.
 */
static const struct mpq_framing_pattern_lookup_params
		mpeg2_patterns[MPQ_MPEG2_PATTERN_NUM] = {
	{{0x00, 0x00, 0x01, 0xB3}, {0xFF, 0xFF, 0xFF, 0xFF}, 4,
			DMX_FRM_MPEG2_SEQUENCE_HEADER},
	{{0x00, 0x00, 0x01, 0xB8}, {0xFF, 0xFF, 0xFF, 0xFF}, 4,
			DMX_FRM_MPEG2_GOP_HEADER},
	{{0x00, 0x00, 0x01, 0x00, 0x00, 0x08},
			{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x38}, 6,
			DMX_FRM_MPEG2_I_PIC},
	{{0x00, 0x00, 0x01, 0x00, 0x00, 0x10},
			{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x38}, 6,
			DMX_FRM_MPEG2_P_PIC},
	{{0x00, 0x00, 0x01, 0x00, 0x00, 0x18},
			{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x38}, 6,
			DMX_FRM_MPEG2_B_PIC}
};

static const struct mpq_framing_pattern_lookup_params
		h264_patterns[MPQ_H264_PATTERN_NUM] = {
	{{0x00, 0x00, 0x01, 0x07}, {0xFF, 0xFF, 0xFF, 0x1F}, 4,
			DMX_FRM_H264_SPS},
	{{0x00, 0x00, 0x01, 0x08}, {0xFF, 0xFF, 0xFF, 0x1F}, 4,
			DMX_FRM_H264_PPS},
	{{0x00, 0x00, 0x01, 0x05, 0x80}, {0xFF, 0xFF, 0xFF, 0x1F, 0x80}, 5,
			DMX_FRM_H264_IDR_PIC},
	{{0x00, 0x00, 0x01, 0x01, 0x80}, {0xFF, 0xFF, 0xFF, 0x1F, 0x80}, 5,
			DMX_FRM_H264_NON_IDR_PIC}
};

static const struct mpq_framing_pattern_lookup_params
		vc1_patterns[MPQ_VC1_PATTERN_NUM] = {
	{{0x00, 0x00, 0x01, 0x0F}, {0xFF, 0xFF, 0xFF, 0xFF}, 4,
			DMX_FRM_VC1_SEQUENCE_HEADER},
	{{0x00, 0x00, 0x01, 0x0E}, {0xFF, 0xFF, 0xFF, 0xFF}, 4,
			DMX_FRM_VC1_ENTRY_POINT_HEADER},
	{{0x00, 0x00, 0x01, 0x0D}, {0xFF, 0xFF, 0xFF, 0xFF}, 4,
			DMX_FRM_VC1_FRAME_START_CODE}
};

/* Global data-structure for managing demux devices */
static struct
{
	/* ION demux client used for memory allocation */
	struct ion_client *ion_client;

	/* demux devices array */
	struct mpq_demux *devices;

	/* Stream buffers objects used for tunneling to decoders */
	struct mpq_streambuffer
		decoder_buffers[MPQ_ADAPTER_MAX_NUM_OF_INTERFACES];

	/*
	 * Indicates whether the video decoder handles framing
	 * or we are required to provide framing information
	 * in the meta-data passed to the decoder.
	 */
	int decoder_framing;
} mpq_dmx_info;

/* Check that PES header is valid and that it is a video PES */
static int mpq_dmx_is_valid_video_pes(struct pes_packet_header *pes_header)
{
	/* start-code valid? */
	if ((pes_header->packet_start_code_prefix_1 != 0) ||
		(pes_header->packet_start_code_prefix_2 != 0) ||
		(pes_header->packet_start_code_prefix_3 != 1))
		return -EINVAL;

	/* stream_id is video? */
	if ((pes_header->stream_id & 0xF0) != 0xE0)
		return -EINVAL;

	return 0;
}

/* Check if a framing pattern is a video frame pattern or a header pattern */
static inline int mpq_dmx_is_video_frame(
				enum dmx_indexing_video_standard standard,
				enum dmx_framing_pattern_type pattern_type)
{
	switch (standard) {
	case DMX_INDEXING_MPEG2:
		if ((pattern_type == DMX_FRM_MPEG2_I_PIC) ||
			(pattern_type == DMX_FRM_MPEG2_P_PIC) ||
			(pattern_type == DMX_FRM_MPEG2_B_PIC))
			return 1;
		return 0;
	case DMX_INDEXING_H264:
		if ((pattern_type == DMX_FRM_H264_IDR_PIC) ||
			(pattern_type == DMX_FRM_H264_NON_IDR_PIC))
			return 1;
		return 0;
	case DMX_INDEXING_VC1:
		if (pattern_type == DMX_FRM_VC1_FRAME_START_CODE)
			return 1;
		return 0;
	default:
		return -EINVAL;
	}
}

/*
 * mpq_framing_pattern_lookup_results - framing lookup results
 *
 * @offset: The offset in the buffer where the pattern was found.
 * If a pattern is found using a prefix (i.e. started on the
 * previous buffer), offset is zero.
 * @type: the type of the pattern found.
 * @used_prefix_size: the prefix size that was used to find this pattern
 */
struct mpq_framing_pattern_lookup_results {
	struct {
		u32 offset;
		enum dmx_framing_pattern_type type;
		u32 used_prefix_size;
	} info[MPQ_MAX_FOUND_PATTERNS];
};

/*
 * Check if two patterns are identical, taking mask into consideration.
 * @pattern1: the first byte pattern to compare.
 * @pattern2: the second byte pattern to compare.
 * @mask: the bit mask to use.
 * @pattern_size: the length of both patterns and the mask, in bytes.
 *
 * Return: 1 if patterns match, 0 otherwise.
 */
static inline int mpq_dmx_patterns_match(const u8 *pattern1, const u8 *pattern2,
					const u8 *mask, size_t pattern_size)
{
	int i;

	/*
	 * Assumption: it is OK to access pattern1, pattern2 and mask.
	 * This function performs no sanity checks to keep things fast.
	 */

	for (i = 0; i < pattern_size; i++)
		if ((pattern1[i] & mask[i]) != (pattern2[i] & mask[i]))
			return 0;

	return 1;
}

/*
 * mpq_dmx_framing_pattern_search -
 * search for framing patterns in a given buffer.
 *
 * Optimized version: first search for a common substring, e.g. 0x00 0x00 0x01.
 * If this string is found, go over all the given patterns (all must start
 * with this string) and search for their ending in the buffer.
 *
 * Assumption: the patterns we look for do not spread over more than two
 * buffers.
 *
 * @paterns: the full patterns information to look for.
 * @patterns_num: the number of patterns to look for.
 * @buf: the buffer to search.
 * @buf_size: the size of the buffer to search. we search the entire buffer.
 * @prefix_size_masks: a bit mask (per pattern) of possible prefix sizes to use
 * when searching for a pattern that started at the last buffer.
 * Updated in this function for use in the next lookup.
 * @results: lookup results (offset, type, used_prefix_size) per found pattern,
 * up to MPQ_MAX_FOUND_PATTERNS.
 *
 * Return:
 *   Number of patterns found (up to MPQ_MAX_FOUND_PATTERNS).
 *   0 if pattern was not found.
 *   Negative error value on failure.
 */
static int mpq_dmx_framing_pattern_search(
		const struct mpq_framing_pattern_lookup_params *patterns,
		int patterns_num,
		const u8 *buf,
		size_t buf_size,
		struct mpq_framing_prefix_size_masks *prefix_size_masks,
		struct mpq_framing_pattern_lookup_results *results)
{
	int i, j;
	unsigned int current_size;
	u32 prefix;
	int found = 0;
	int start_offset = 0;
	/* the starting common substring to look for */
	u8 string[] = {0x00, 0x00, 0x01};
	/* the mask for the starting string */
	u8 string_mask[] = {0xFF, 0xFF, 0xFF};
	/* the size of the starting string (in bytes) */
	size_t string_size = 3;

	/* sanity checks - can be commented out for optimization purposes */
	if ((patterns == NULL) || (patterns_num <= 0) || (buf == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	memset(results, 0, sizeof(struct mpq_framing_pattern_lookup_results));

	/*
	 * handle prefix - disregard string, simply check all patterns,
	 * looking for a matching suffix at the very beginning of the buffer.
	 */
	for (j = 0; (j < patterns_num) && !found; j++) {
		prefix = prefix_size_masks->size_mask[j];
		current_size = 32;
		while (prefix) {
			if (prefix & (0x1 << (current_size - 1))) {
				/*
				 * check that we don't look further
				 * than buf_size boundary
				 */
				if ((int)(patterns[j].size - current_size) >
						buf_size)
					break;

				if (mpq_dmx_patterns_match(
					(patterns[j].pattern + current_size),
					buf, (patterns[j].mask + current_size),
					(patterns[j].size - current_size))) {

					MPQ_DVB_DBG_PRINT(
						"%s: Found matching pattern using prefix of size %d\n",
						__func__, current_size);
					/*
					 * pattern found using prefix at the
					 * very beginning of the buffer, so
					 * offset is 0, but we already zeroed
					 * everything in the beginning of the
					 * function. that's why the next line
					 * is commented.
					 */
					/* results->info[found].offset = 0; */
					results->info[found].type =
							patterns[j].type;
					results->info[found].used_prefix_size =
							current_size;
					found++;
					/*
					 * save offset to start looking from
					 * in the buffer, to avoid reusing the
					 * data of a pattern we already found.
					 */
					start_offset = (patterns[j].size -
							current_size);

					if (found >= MPQ_MAX_FOUND_PATTERNS)
						goto next_prefix_lookup;
					/*
					 * we don't want to search for the same
					 * pattern with several possible prefix
					 * sizes if we have already found it,
					 * so we break from the inner loop.
					 * since we incremented 'found', we
					 * will not search for additional
					 * patterns using a prefix - that would
					 * imply ambiguous patterns where one
					 * pattern can be included in another.
					 * the for loop will exit.
					 */
					break;
				}
			}
			current_size--;
			prefix &= ~(0x1 << (current_size - 1));
		}
	}

	/*
	 * Search buffer for entire pattern, starting with the string.
	 * Note the external for loop does not execute if buf_size is
	 * smaller than string_size (the cast to int is required, since
	 * size_t is unsigned).
	 */
	for (i = start_offset; i < (int)(buf_size - string_size + 1); i++) {
		if (mpq_dmx_patterns_match(string, (buf + i), string_mask,
							string_size)) {
			/* now search for patterns: */
			for (j = 0; j < patterns_num; j++) {
				/* avoid overflow to next buffer */
				if ((i + patterns[j].size) > buf_size)
					continue;

				if (mpq_dmx_patterns_match(
					(patterns[j].pattern + string_size),
					(buf + i + string_size),
					(patterns[j].mask + string_size),
					(patterns[j].size - string_size))) {

					results->info[found].offset = i;
					results->info[found].type =
						patterns[j].type;
					/*
					 * save offset to start next prefix
					 * lookup, to avoid reusing the data
					 * of any pattern we already found.
					 */
					if ((i + patterns[j].size) >
							start_offset)
						start_offset = (i +
							patterns[j].size);
					/*
					 * did not use a prefix to find this
					 * pattern, but we zeroed everything
					 * in the beginning of the function.
					 * So no need to zero used_prefix_size
					 * for results->info[found]
					 */

					found++;
					if (found >= MPQ_MAX_FOUND_PATTERNS)
						goto next_prefix_lookup;
					/*
					 * theoretically we don't have to break
					 * here, but we don't want to search
					 * for the other matching patterns on
					 * the very same same place in the
					 * buffer. That would mean the
					 * (pattern & mask) combinations are
					 * not unique. So we break from inner
					 * loop and move on to the next place
					 * in the buffer.
					 */
					break;
				}
			}
		}
	}

next_prefix_lookup:
	/* check for possible prefix sizes for the next buffer */
	for (j = 0; j < patterns_num; j++) {
		prefix_size_masks->size_mask[j] = 0;
		for (i = 1; i < patterns[j].size; i++) {
			/*
			 * avoid looking outside of the buffer
			 * or reusing previously used data.
			 */
			if (i > (buf_size - start_offset))
				break;

			if (mpq_dmx_patterns_match(patterns[j].pattern,
					(buf + buf_size - i),
					patterns[j].mask, i)) {
				prefix_size_masks->size_mask[j] |=
						(1 << (i - 1));
			}
		}
	}

	return found;
}

/*
 * mpq_dmx_get_pattern_params -
 * get a pointer to the relevant pattern parameters structure,
 * based on the video parameters.
 *
 * @video_params: the video parameters (e.g. video standard).
 * @patterns: a pointer to a pointer to the pattern parameters,
 * updated by this function.
 * @patterns_num: number of patterns, updated by this function.
 */
static inline int mpq_dmx_get_pattern_params(
		struct dmx_indexing_video_params *video_params,
		const struct mpq_framing_pattern_lookup_params **patterns,
		int *patterns_num)
{
	switch (video_params->standard) {
	case DMX_INDEXING_MPEG2:
		*patterns = mpeg2_patterns;
		*patterns_num = MPQ_MPEG2_PATTERN_NUM;
		break;
	case DMX_INDEXING_H264:
		*patterns = h264_patterns;
		*patterns_num = MPQ_H264_PATTERN_NUM;
		break;
	case DMX_INDEXING_VC1:
		*patterns = vc1_patterns;
		*patterns_num = MPQ_VC1_PATTERN_NUM;
		break;
	default:
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		*patterns = NULL;
		*patterns_num = 0;
		return -EINVAL;
	}

	return 0;
}

/* Extend dvb-demux debugfs with HW statistics */
void mpq_dmx_init_hw_statistics(struct mpq_demux *mpq_demux)
{
	/*
	 * Extend dvb-demux debugfs with HW statistics.
	 * Note that destruction of debugfs directory is done
	 * when dvb-demux is terminated.
	 */
	mpq_demux->hw_notification_count = 0;
	mpq_demux->hw_notification_interval = 0;
	mpq_demux->hw_notification_size = 0;
	mpq_demux->hw_notification_min_size = 0xFFFFFFFF;

	if (mpq_demux->demux.dmx.debugfs_demux_dir != NULL) {
		debugfs_create_u32(
			"hw_notification_interval",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->hw_notification_interval);

		debugfs_create_u32(
			"hw_notification_min_interval",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->hw_notification_min_interval);

		debugfs_create_u32(
			"hw_notification_count",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->hw_notification_count);

		debugfs_create_u32(
			"hw_notification_size",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->hw_notification_size);

		debugfs_create_u32(
			"hw_notification_min_size",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->hw_notification_min_size);

		debugfs_create_u32(
			"decoder_drop_count",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->decoder_drop_count);

		debugfs_create_u32(
			"decoder_out_count",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->decoder_out_count);

		debugfs_create_u32(
			"decoder_out_interval_sum",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->decoder_out_interval_sum);

		debugfs_create_u32(
			"decoder_out_interval_average",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->decoder_out_interval_average);

		debugfs_create_u32(
			"decoder_out_interval_max",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->decoder_out_interval_max);

		debugfs_create_u32(
			"decoder_ts_errors",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.dmx.debugfs_demux_dir,
			&mpq_demux->decoder_ts_errors);
	}
}
EXPORT_SYMBOL(mpq_dmx_init_hw_statistics);


/* Update dvb-demux debugfs with HW notification statistics */
void mpq_dmx_update_hw_statistics(struct mpq_demux *mpq_demux)
{
	struct timespec curr_time, delta_time;
	u64 delta_time_ms;

	curr_time = current_kernel_time();
	if (likely(mpq_demux->hw_notification_count)) {
		/* calculate time-delta between notifications */
		delta_time =
			timespec_sub(
					curr_time,
					mpq_demux->last_notification_time);

		delta_time_ms = ((u64)delta_time.tv_sec * MSEC_PER_SEC) +
					delta_time.tv_nsec / NSEC_PER_MSEC;

		mpq_demux->hw_notification_interval = delta_time_ms;

		if ((mpq_demux->hw_notification_count == 1) ||
			(mpq_demux->hw_notification_interval &&
			 mpq_demux->hw_notification_interval <
				mpq_demux->hw_notification_min_interval))
			mpq_demux->hw_notification_min_interval =
				mpq_demux->hw_notification_interval;
	}

	mpq_demux->hw_notification_count++;
	mpq_demux->last_notification_time = curr_time;
}
EXPORT_SYMBOL(mpq_dmx_update_hw_statistics);


int mpq_dmx_plugin_init(mpq_dmx_init dmx_init_func)
{
	int i;
	int result;
	struct mpq_demux *mpq_demux;
	struct dvb_adapter *mpq_adapter;

	MPQ_DVB_DBG_PRINT("%s executed, device num %d\n",
					  __func__,
					  mpq_demux_device_num);

	mpq_adapter = mpq_adapter_get();

	if (mpq_adapter == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_adapter is not valid\n",
			__func__);
		result = -EPERM;
		goto init_failed;
	}

	if (mpq_demux_device_num == 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_demux_device_num set to 0\n",
			__func__);

		result = -EPERM;
		goto init_failed;
	}

	mpq_dmx_info.devices = NULL;
	mpq_dmx_info.ion_client = NULL;

	/*
	 * TODO: the following should be set based on the decoder:
	 * 0 means the decoder doesn't handle framing, so framing
	 * is done by demux. 1 means the decoder handles framing.
	 */
	mpq_dmx_info.decoder_framing = 0;

	/* Allocate memory for all MPQ devices */
	mpq_dmx_info.devices =
		vmalloc(mpq_demux_device_num*sizeof(struct mpq_demux));

	if (!mpq_dmx_info.devices) {
		MPQ_DVB_ERR_PRINT(
				"%s: failed to allocate devices memory\n",
				__func__);

		result = -ENOMEM;
		goto init_failed;
	}

	/* Zero allocated memory */
	memset(mpq_dmx_info.devices,
		   0,
		   mpq_demux_device_num*sizeof(struct mpq_demux));

	/*
	 * Create a new ION client used by demux to allocate memory
	 * for decoder's buffers.
	 */
	mpq_dmx_info.ion_client =
		msm_ion_client_create(UINT_MAX, "demux_client");

	if (IS_ERR_OR_NULL(mpq_dmx_info.ion_client)) {
		MPQ_DVB_ERR_PRINT(
				"%s: msm_ion_client_create\n",
				__func__);

		result = PTR_ERR(mpq_dmx_info.ion_client);
		if (!result)
			result = -ENOMEM;
		mpq_dmx_info.ion_client = NULL;
		goto init_failed_free_demux_devices;
	}

	/* Initialize and register all demux devices to the system */
	for (i = 0; i < mpq_demux_device_num; i++) {
		mpq_demux = mpq_dmx_info.devices+i;

		/* initialize demux source to memory by default */
		mpq_demux->source = DMX_SOURCE_DVR0 + i;

		/*
		 * Give the plugin pointer to the ion client so
		 * that it can allocate memory from ION if it requires so
		 */
		mpq_demux->ion_client = mpq_dmx_info.ion_client;

		spin_lock_init(&mpq_demux->feed_lock);

		/*
		 * mpq_demux_plugin_hw_init should be implemented
		 * by the specific plugin
		 */
		result = dmx_init_func(mpq_adapter, mpq_demux);
		if (result < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: dmx_init_func (errno=%d)\n",
				__func__,
				result);

			goto init_failed_free_demux_devices;
		}

		mpq_demux->is_initialized = 1;

		/*
		 * Add capability of receiving input from memory.
		 * Every demux in our system may be connected to memory input,
		 * or any live input.
		 */
		mpq_demux->fe_memory.source = DMX_MEMORY_FE;
		result =
			mpq_demux->demux.dmx.add_frontend(
					&mpq_demux->demux.dmx,
					&mpq_demux->fe_memory);

		if (result < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: add_frontend (mem) failed (errno=%d)\n",
				__func__,
				result);

			goto init_failed_free_demux_devices;
		}
	}

	return 0;

init_failed_free_demux_devices:
	mpq_dmx_plugin_exit();
init_failed:
	return result;
}
EXPORT_SYMBOL(mpq_dmx_plugin_init);


void mpq_dmx_plugin_exit(void)
{
	int i;
	struct mpq_demux *mpq_demux;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	if (mpq_dmx_info.ion_client != NULL) {
		ion_client_destroy(mpq_dmx_info.ion_client);
		mpq_dmx_info.ion_client = NULL;
	}

	if (mpq_dmx_info.devices != NULL) {
		for (i = 0; i < mpq_demux_device_num; i++) {
			mpq_demux = mpq_dmx_info.devices + i;

			if (mpq_demux->is_initialized) {
				mpq_demux->demux.dmx.remove_frontend(
							&mpq_demux->demux.dmx,
							&mpq_demux->fe_memory);

				dvb_dmxdev_release(&mpq_demux->dmxdev);
				dvb_dmx_release(&mpq_demux->demux);
			}
		}

		vfree(mpq_dmx_info.devices);
		mpq_dmx_info.devices = NULL;
	}
}
EXPORT_SYMBOL(mpq_dmx_plugin_exit);


int mpq_dmx_set_source(
		struct dmx_demux *demux,
		const dmx_source_t *src)
{
	int i;
	int dvr_index;
	int dmx_index;
	struct dvb_demux *dvb_demux = demux->priv;
	struct mpq_demux *mpq_demux;

	if ((mpq_dmx_info.devices == NULL) || (dvb_demux == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mpq_demux = dvb_demux->priv;
	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	/*
	 * For dvr sources,
	 * verify that this source is connected to the respective demux
	 */
	dmx_index = mpq_demux - mpq_dmx_info.devices;

	if (*src >= DMX_SOURCE_DVR0) {
		dvr_index = *src - DMX_SOURCE_DVR0;

		if (dvr_index != dmx_index) {
			MPQ_DVB_ERR_PRINT(
				"%s: can't connect demux%d to dvr%d\n",
				__func__,
				dmx_index,
				dvr_index);
			return -EINVAL;
		}
	}

	/*
	 * For front-end sources,
	 * verify that this source is not already set to different demux
	 */
	for (i = 0; i < mpq_demux_device_num; i++) {
		if ((&mpq_dmx_info.devices[i] != mpq_demux) &&
			(mpq_dmx_info.devices[i].source == *src)) {
			MPQ_DVB_ERR_PRINT(
				"%s: demux%d source can't be set,\n"
				"demux%d occupies this source already\n",
				__func__,
				dmx_index,
				i);
			return -EBUSY;
		}
	}

	mpq_demux->source = *src;
	return 0;
}
EXPORT_SYMBOL(mpq_dmx_set_source);

/**
 * Takes an ION allocated buffer's file descriptor and handles the details of
 * mapping it into kernel memory and obtaining an ION handle for it.
 * Internal helper function.
 *
 * @client: ION client
 * @handle: ION file descriptor to map
 * @priv_handle: returned ION handle. Must be freed when no longer needed
 * @kernel_mem: returned kernel mapped pointer
 *
 * Note: mapping might not be possible in secured heaps/buffers, and so NULL
 * might be returned in kernel_mem
 *
 * Return errors status
 */
static int mpq_map_buffer_to_kernel(
	struct ion_client *client,
	int handle,
	struct ion_handle **priv_handle,
	void **kernel_mem)
{
	struct ion_handle *ion_handle;
	unsigned long ionflag = 0;
	int ret;

	if (NULL == client || priv_handle == NULL || kernel_mem == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	ion_handle = ion_import_dma_buf(client, handle);
	if (IS_ERR_OR_NULL(ion_handle)) {
		ret = PTR_ERR(ion_handle);
		MPQ_DVB_ERR_PRINT("%s: ion_import_dma_buf failed %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;

		goto map_buffer_failed;
	}

	ret = ion_handle_get_flags(client, ion_handle, &ionflag);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: ion_handle_get_flags failed %d\n",
			__func__, ret);
		goto map_buffer_failed_free_buff;
	}

	if (ionflag & ION_SECURE) {
		MPQ_DVB_DBG_PRINT("%s: secured buffer\n", __func__);
		*kernel_mem = NULL;
	} else {
		*kernel_mem = ion_map_kernel(client, ion_handle);
		if (*kernel_mem == NULL) {
			MPQ_DVB_ERR_PRINT("%s: ion_map_kernel failed\n",
				__func__);
			ret = -ENOMEM;
			goto map_buffer_failed_free_buff;
		}
	}

	*priv_handle = ion_handle;
	return 0;

map_buffer_failed_free_buff:
	ion_free(client, ion_handle);
map_buffer_failed:
	return ret;
}

int mpq_dmx_map_buffer(struct dmx_demux *demux, struct dmx_buffer *dmx_buffer,
		void **priv_handle, void **kernel_mem)
{
	struct dvb_demux *dvb_demux = demux->priv;
	struct mpq_demux *mpq_demux;

	if ((mpq_dmx_info.devices == NULL) || (dvb_demux == NULL) ||
		(priv_handle == NULL) || (kernel_mem == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mpq_demux = dvb_demux->priv;
	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	return mpq_map_buffer_to_kernel(
		mpq_demux->ion_client,
		dmx_buffer->handle,
		(struct ion_handle **)priv_handle, kernel_mem);
}
EXPORT_SYMBOL(mpq_dmx_map_buffer);

int mpq_dmx_unmap_buffer(struct dmx_demux *demux,
		void *priv_handle)
{
	struct dvb_demux *dvb_demux = demux->priv;
	struct ion_handle *ion_handle = priv_handle;
	struct mpq_demux *mpq_demux;
	unsigned long ionflag = 0;
	int ret;

	if ((mpq_dmx_info.devices == NULL) || (dvb_demux == NULL) ||
		(priv_handle == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mpq_demux = dvb_demux->priv;
	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	ret = ion_handle_get_flags(mpq_demux->ion_client, ion_handle, &ionflag);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: ion_handle_get_flags failed %d\n",
			__func__, ret);
		return -EINVAL;
	}

	if (!(ionflag & ION_SECURE))
		ion_unmap_kernel(mpq_demux->ion_client, ion_handle);

	ion_free(mpq_demux->ion_client, ion_handle);

	return 0;
}
EXPORT_SYMBOL(mpq_dmx_unmap_buffer);

int mpq_dmx_reuse_decoder_buffer(struct dvb_demux_feed *feed, int cookie)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;

	if (!generate_es_events) {
		MPQ_DVB_ERR_PRINT(
			"%s: Cannot release decoder buffer when not working with new elementary stream data events\n",
			__func__);
		return -EPERM;
	}

	if (cookie < 0) {
		MPQ_DVB_ERR_PRINT("%s: invalid cookie parameter\n", __func__);
		return -EINVAL;
	}

	if (mpq_dmx_is_video_feed(feed)) {
		struct mpq_video_feed_info *feed_data;
		struct mpq_streambuffer *stream_buffer;
		int ret;

		spin_lock(&mpq_demux->feed_lock);

		if (feed->priv == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: invalid feed, feed->priv is NULL\n",
				__func__);
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

		feed_data = feed->priv;
		stream_buffer = feed_data->video_buffer;
		if (stream_buffer == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: invalid feed, feed_data->video_buffer is NULL\n",
				__func__);
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

		ret = mpq_streambuffer_pkt_dispose(stream_buffer, cookie, 1);

		spin_unlock(&mpq_demux->feed_lock);

		return ret;
	}

	/* else */
	MPQ_DVB_ERR_PRINT("%s: Invalid feed type %d\n",
			__func__, feed->pes_type);

	return -EINVAL;
}
EXPORT_SYMBOL(mpq_dmx_reuse_decoder_buffer);

/**
 * Handles the details of internal decoder buffer allocation via ION.
 * Internal helper function.
 * @feed_data: decoder feed object
 * @dec_buffs: buffer information
 * @client: ION client
 *
 * Return error status
 */
static int mpq_dmx_init_internal_buffers(
	struct mpq_video_feed_info *feed_data,
	struct dmx_decoder_buffers *dec_buffs,
	struct ion_client *client)
{
	struct ion_handle *temp_handle = NULL;
	void *payload_buffer = NULL;
	int actual_buffer_size = 0;
	int ret = 0;

	MPQ_DVB_DBG_PRINT("%s: Internal decoder buffer allocation\n", __func__);

	actual_buffer_size = dec_buffs->buffers_size;
	actual_buffer_size += (SZ_4K - 1);
	actual_buffer_size &= ~(SZ_4K - 1);

	temp_handle = ion_alloc(client, actual_buffer_size, SZ_4K,
		ION_HEAP(video_secure_ion_heap) |
		ION_HEAP(video_nonsecure_ion_heap),
		ION_FLAG_CACHED);

	if (IS_ERR_OR_NULL(temp_handle)) {
		ret = PTR_ERR(temp_handle);
		MPQ_DVB_ERR_PRINT("%s: FAILED to allocate payload buffer %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;
		goto end;
	}

	payload_buffer = ion_map_kernel(client, temp_handle);

	if (IS_ERR_OR_NULL(payload_buffer)) {
		ret = PTR_ERR(payload_buffer);
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to map payload buffer %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;
		goto init_failed_free_payload_buffer;
	}
	feed_data->buffer_desc.decoder_buffers_num = 1;
	feed_data->buffer_desc.ion_handle[0] = temp_handle;
	feed_data->buffer_desc.desc[0].base = payload_buffer;
	feed_data->buffer_desc.desc[0].size = actual_buffer_size;
	feed_data->buffer_desc.desc[0].read_ptr = 0;
	feed_data->buffer_desc.desc[0].write_ptr = 0;
	feed_data->buffer_desc.desc[0].handle =
		ion_share_dma_buf(client, temp_handle);
	if (IS_ERR_VALUE(feed_data->buffer_desc.desc[0].handle)) {
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to share payload buffer %d\n",
			__func__, ret);
		ret = -ENOMEM;
		goto init_failed_unmap_payload_buffer;
	}

	return 0;

init_failed_unmap_payload_buffer:
	ion_unmap_kernel(client, temp_handle);
	feed_data->buffer_desc.desc[0].base = NULL;
init_failed_free_payload_buffer:
	ion_free(client, temp_handle);
	feed_data->buffer_desc.ion_handle[0] = NULL;
	feed_data->buffer_desc.desc[0].size = 0;
	feed_data->buffer_desc.decoder_buffers_num = 0;
end:
	return ret;
}

/**
 * Handles the details of external decoder buffers allocated by user.
 * Each buffer is mapped into kernel memory and an ION handle is obtained, and
 * decoder feed object is updated with related information.
 * Internal helper function.
 * @feed_data: decoder feed object
 * @dec_buffs: buffer information
 * @client: ION client
 *
 * Return error status
 */
static int mpq_dmx_init_external_buffers(
	struct mpq_video_feed_info *feed_data,
	struct dmx_decoder_buffers *dec_buffs,
	struct ion_client *client)
{
	struct ion_handle *temp_handle = NULL;
	void *payload_buffer = NULL;
	int actual_buffer_size = 0;
	int ret = 0;
	int i;

	/*
	 * Payload buffer was allocated externally (through ION).
	 * Map the ion handles to kernel memory
	 */
	MPQ_DVB_DBG_PRINT("%s: External decoder buffer allocation\n", __func__);

	actual_buffer_size = dec_buffs->buffers_size;
	if (!dec_buffs->is_linear) {
		MPQ_DVB_DBG_PRINT("%s: Ex. Ring-buffer\n", __func__);
		feed_data->buffer_desc.decoder_buffers_num = 1;
	} else {
		MPQ_DVB_DBG_PRINT("%s: Ex. Linear\n", __func__);
		feed_data->buffer_desc.decoder_buffers_num =
			dec_buffs->buffers_num;
	}

	for (i = 0; i < feed_data->buffer_desc.decoder_buffers_num; i++) {
		ret = mpq_map_buffer_to_kernel(
			client,
			dec_buffs->handles[i],
			&temp_handle,
			&payload_buffer);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: Failed mapping buffer %d\n",
				__func__, i);
			goto init_failed;
		}
		feed_data->buffer_desc.ion_handle[i] = temp_handle;
		feed_data->buffer_desc.desc[i].base = payload_buffer;
		feed_data->buffer_desc.desc[i].handle =
			dec_buffs->handles[i];
		feed_data->buffer_desc.desc[i].size =
			dec_buffs->buffers_size;
		feed_data->buffer_desc.desc[i].read_ptr = 0;
		feed_data->buffer_desc.desc[i].write_ptr = 0;

		MPQ_DVB_DBG_PRINT(
			"%s: Buffer #%d: base=0x%p, handle=%d, size=%d\n",
			__func__, i ,
			feed_data->buffer_desc.desc[i].base,
			feed_data->buffer_desc.desc[i].handle,
			feed_data->buffer_desc.desc[i].size);
	}

	return 0;

init_failed:
	for (i = 0; i < feed_data->buffer_desc.decoder_buffers_num; i++) {
		if (feed_data->buffer_desc.ion_handle[i]) {
			if (feed_data->buffer_desc.desc[i].base) {
				ion_unmap_kernel(client,
					feed_data->buffer_desc.ion_handle[i]);
				feed_data->buffer_desc.desc[i].base = NULL;
			}
			ion_free(client, feed_data->buffer_desc.ion_handle[i]);
			feed_data->buffer_desc.ion_handle[i] = NULL;
			feed_data->buffer_desc.desc[i].size = 0;
		}
	}
	return ret;
}

/**
 * Handles the details of initializing the mpq_streambuffer object according
 * to the user decoder buffer configuration: External/Internal buffers and
 * ring/linear buffering mode.
 * Internal helper function.
 * @feed:  dvb demux feed object, contains the buffers configuration
 * @feed_data: decoder feed object
 * @stream_buffer: stream buffer object to initialize
 *
 * Return error status
 */
static int mpq_dmx_init_streambuffer(
	struct dvb_demux_feed *feed,
	struct mpq_video_feed_info *feed_data,
	struct mpq_streambuffer *stream_buffer)
{
	int ret;
	void *packet_buffer = NULL;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct ion_client *client = mpq_demux->ion_client;
	struct dmx_decoder_buffers *dec_buffs = NULL;
	enum mpq_streambuffer_mode mode;

	dec_buffs = feed->feed.ts.decoder_buffers;

	/* Allocate packet buffer holding the meta-data */
	packet_buffer = vmalloc(VIDEO_META_DATA_BUFFER_SIZE);

	if (packet_buffer == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to allocate packets buffer\n",
			__func__);

		ret = -ENOMEM;
		goto end;
	}

	MPQ_DVB_DBG_PRINT("%s: dec_buffs: num=%d, size=%d, linear=%d\n",
			__func__,
			dec_buffs->buffers_num,
			dec_buffs->buffers_size,
			dec_buffs->is_linear);

	feed_data->buffer_desc.decoder_buffers_num = dec_buffs->buffers_num;
	if (0 == dec_buffs->buffers_num)
		ret = mpq_dmx_init_internal_buffers(
			feed_data, dec_buffs, client);
	else
		ret = mpq_dmx_init_external_buffers(
			feed_data, dec_buffs, client);

	if (ret != 0)
		goto init_failed_free_packet_buffer;

	mode = dec_buffs->is_linear ? MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR :
		MPQ_STREAMBUFFER_BUFFER_MODE_RING;
	ret = mpq_streambuffer_init(
			feed_data->video_buffer,
			mode,
			feed_data->buffer_desc.desc,
			feed_data->buffer_desc.decoder_buffers_num,
			packet_buffer,
			VIDEO_META_DATA_BUFFER_SIZE);

	if (ret != 0)
		goto init_failed_free_packet_buffer;

	goto end;


init_failed_free_packet_buffer:
	vfree(packet_buffer);
end:
	return ret;
}

static void mpq_dmx_release_streambuffer(
	struct dvb_demux_feed *feed,
	struct mpq_video_feed_info *feed_data,
	struct ion_client *client)
{
	int buf_num = 0;
	int i;
	struct dmx_decoder_buffers *dec_buffs = feed->feed.ts.decoder_buffers;

	mpq_adapter_unregister_stream_if(feed_data->stream_interface);

	vfree(feed_data->video_buffer->packet_data.data);

	buf_num = feed_data->buffer_desc.decoder_buffers_num;

	for (i = 0; i < buf_num; i++) {
		if (feed_data->buffer_desc.ion_handle[i]) {
			if (feed_data->buffer_desc.desc[i].base) {
				ion_unmap_kernel(client,
					feed_data->buffer_desc.ion_handle[i]);
				feed_data->buffer_desc.desc[i].base = NULL;
			}

			/*
			 * Un-share the buffer if kernel it the one that
			 * shared it.
			 */
			if (0 == dec_buffs->buffers_num) {
				struct file *shared_file = fget(
					feed_data->buffer_desc.desc[i].handle);

				if (shared_file)
					fput(shared_file);
				else
					MPQ_DVB_ERR_PRINT(
						"%s: failed to get shared-file handle\n",
						__func__);
			}

			ion_free(client, feed_data->buffer_desc.ion_handle[i]);
			feed_data->buffer_desc.ion_handle[i] = NULL;
			feed_data->buffer_desc.desc[i].size = 0;
		}
	}
}

int mpq_dmx_init_video_feed(struct dvb_demux_feed *feed)
{
	int ret;
	struct mpq_video_feed_info *feed_data;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_streambuffer *stream_buffer;

	/* Allocate memory for private feed data */
	feed_data = vzalloc(sizeof(struct mpq_video_feed_info));

	if (feed_data == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to allocate private video feed data\n",
			__func__);

		ret = -ENOMEM;
		goto init_failed;
	}

	/* get and store framing information if required */
	if (!mpq_dmx_info.decoder_framing) {
		mpq_dmx_get_pattern_params(&feed->indexing_params,
				&feed_data->patterns, &feed_data->patterns_num);
		if (feed_data->patterns == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: FAILED to get framing pattern parameters\n",
				__func__);

			ret = -EINVAL;
			goto init_failed_free_priv_data;
		}
	}

	/* Register the new stream-buffer interface to MPQ adapter */
	switch (feed->pes_type) {
	case DMX_TS_PES_VIDEO0:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO0_STREAM_IF;
		break;

	case DMX_TS_PES_VIDEO1:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO1_STREAM_IF;
		break;

	case DMX_TS_PES_VIDEO2:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO2_STREAM_IF;
		break;

	case DMX_TS_PES_VIDEO3:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO3_STREAM_IF;
		break;

	default:
		MPQ_DVB_ERR_PRINT(
			"%s: Invalid pes type %d\n",
			__func__,
			feed->pes_type);
		ret = -EINVAL;
		goto init_failed_free_priv_data;
	}

	/* make sure not occupied already */
	stream_buffer = NULL;
	mpq_adapter_get_stream_if(
			feed_data->stream_interface,
			&stream_buffer);
	if (stream_buffer != NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: Video interface %d already occupied!\n",
			__func__,
			feed_data->stream_interface);
		ret = -EBUSY;
		goto init_failed_free_priv_data;
	}

	feed_data->video_buffer =
		&mpq_dmx_info.decoder_buffers[feed_data->stream_interface];

	ret = mpq_dmx_init_streambuffer(
		feed, feed_data, feed_data->video_buffer);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_init_streambuffer failed, err = %d\n",
			__func__, ret);
		goto init_failed_free_priv_data;
	}

	ret = mpq_adapter_register_stream_if(
			feed_data->stream_interface,
			feed_data->video_buffer);

	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_adapter_register_stream_if failed, "
			"err = %d\n",
			__func__, ret);
		goto init_failed_free_stream_buffer;
	}

	feed_data->pes_payload_address =
		(u32)feed_data->video_buffer->raw_data.data;

	feed_data->pes_header_left_bytes = PES_MANDATORY_FIELDS_LEN;
	feed_data->pes_header_offset = 0;
	feed->pusi_seen = 0;
	feed->peslen = 0;
	feed_data->fullness_wait_cancel = 0;
	mpq_streambuffer_get_data_rw_offset(feed_data->video_buffer, NULL,
		&feed_data->frame_offset);
	feed_data->last_pattern_offset = 0;
	feed_data->pending_pattern_len = 0;
	feed_data->last_framing_match_type = DMX_FRM_UNKNOWN;
	feed_data->found_sequence_header_pattern = 0;
	memset(&feed_data->prefix_size, 0,
			sizeof(struct mpq_framing_prefix_size_masks));
	feed_data->first_prefix_size = 0;
	feed_data->saved_pts_dts_info.pts_exist = 0;
	feed_data->saved_pts_dts_info.dts_exist = 0;
	feed_data->new_pts_dts_info.pts_exist = 0;
	feed_data->new_pts_dts_info.dts_exist = 0;
	feed_data->saved_info_used = 1;
	feed_data->new_info_exists = 0;
	feed_data->first_pts_dts_copy = 1;
	feed_data->tei_errs = 0;
	feed_data->last_continuity = -1;
	feed_data->continuity_errs = 0;
	feed_data->ts_packets_num = 0;
	feed_data->ts_dropped_bytes = 0;
	feed_data->last_pkt_index = -1;

	mpq_demux->decoder_drop_count = 0;
	mpq_demux->decoder_out_count = 0;
	mpq_demux->decoder_out_interval_sum = 0;
	mpq_demux->decoder_out_interval_max = 0;
	mpq_demux->decoder_ts_errors = 0;

	spin_lock(&mpq_demux->feed_lock);
	feed->priv = (void *)feed_data;
	spin_unlock(&mpq_demux->feed_lock);

	return 0;

init_failed_free_stream_buffer:
	mpq_dmx_release_streambuffer(feed, feed_data, mpq_demux->ion_client);
	mpq_adapter_unregister_stream_if(feed_data->stream_interface);
init_failed_free_priv_data:
	vfree(feed_data);
	feed->priv = NULL;
init_failed:

	return ret;
}
EXPORT_SYMBOL(mpq_dmx_init_video_feed);

int mpq_dmx_terminate_video_feed(struct dvb_demux_feed *feed)
{
	struct mpq_video_feed_info *feed_data;
	struct mpq_demux *mpq_demux;

	if (feed->priv == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid feed, feed->priv is NULL\n",
			__func__);

		return -EINVAL;
	}

	mpq_demux = feed->demux->priv;
	feed_data = feed->priv;

	spin_lock(&mpq_demux->feed_lock);
	feed->priv = NULL;
	spin_unlock(&mpq_demux->feed_lock);

	wake_up_all(&feed_data->video_buffer->raw_data.queue);

	mpq_dmx_release_streambuffer(feed, feed_data, mpq_demux->ion_client);

	vfree(feed_data);

	return 0;
}
EXPORT_SYMBOL(mpq_dmx_terminate_video_feed);

int mpq_dmx_decoder_fullness_init(struct dvb_demux_feed *feed)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;

	if (mpq_dmx_is_video_feed(feed)) {
		struct mpq_video_feed_info *feed_data;

		spin_lock(&mpq_demux->feed_lock);

		if (feed->priv == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: invalid feed, feed->priv is NULL\n",
				__func__);
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

		feed_data = feed->priv;
		feed_data->fullness_wait_cancel = 0;

		spin_unlock(&mpq_demux->feed_lock);

		return 0;
	}

	/* else */
	MPQ_DVB_DBG_PRINT(
		"%s: Invalid feed type %d\n",
		__func__,
		feed->pes_type);

	return -EINVAL;
}
EXPORT_SYMBOL(mpq_dmx_decoder_fullness_init);


static inline int mpq_dmx_check_decoder_fullness(
	struct mpq_streambuffer *sbuff,
	size_t required_space)
{
	u32 free = mpq_streambuffer_data_free(sbuff);
	MPQ_DVB_DBG_PRINT("%s: stream buffer free = %d, required = %d\n",
		__func__, free, required_space);

	/*
	 * For linear buffers, verify there's enough space for this TSP
	 * and an additional buffer is free, as framing might required one
	 * more buffer to be available.
	 */
	if (MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR == sbuff->mode)
		return (free >= required_space &&
			sbuff->pending_buffers_count < sbuff->buffers_num-1);
	else
		/* Ring buffer mode */
		return (free >= required_space);
}

int mpq_dmx_decoder_fullness_wait(
		struct dvb_demux_feed *feed,
		size_t required_space)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_streambuffer *sbuff = NULL;
	struct mpq_video_feed_info *feed_data;
	int ret;

	if (!mpq_dmx_is_video_feed(feed)) {
		MPQ_DVB_DBG_PRINT("%s: Invalid feed type %d\n",
			__func__,
			feed->pes_type);
		return -EINVAL;
	}

	spin_lock(&mpq_demux->feed_lock);
	if (feed->priv == NULL) {
		spin_unlock(&mpq_demux->feed_lock);
		return -EINVAL;
	}
	feed_data = feed->priv;
	sbuff = feed_data->video_buffer;
	if (sbuff == NULL) {
		spin_unlock(&mpq_demux->feed_lock);
		MPQ_DVB_ERR_PRINT("%s: mpq_streambuffer object is NULL\n",
			__func__);
		return -EINVAL;
	}

	if ((feed_data != NULL) &&
		(!feed_data->fullness_wait_cancel) &&
		(!mpq_dmx_check_decoder_fullness(sbuff, required_space))) {
		DEFINE_WAIT(__wait);
		for (;;) {
			prepare_to_wait(&sbuff->raw_data.queue,
				&__wait,
				TASK_INTERRUPTIBLE);

			if ((feed->priv == NULL) ||
				feed_data->fullness_wait_cancel ||
				mpq_dmx_check_decoder_fullness(sbuff,
					required_space))
				break;

			if (!signal_pending(current)) {
				spin_unlock(&mpq_demux->feed_lock);
				schedule();
				spin_lock(&mpq_demux->feed_lock);
				continue;
			}

			ret = -ERESTARTSYS;
			break;
		}
		finish_wait(&sbuff->raw_data.queue, &__wait);
	}

	if (ret < 0) {
		spin_unlock(&mpq_demux->feed_lock);
		return ret;
	}

	if ((feed->priv == NULL) || (feed_data->fullness_wait_cancel)) {
		spin_unlock(&mpq_demux->feed_lock);
		return -EINVAL;
	}

	spin_unlock(&mpq_demux->feed_lock);
	return 0;
}
EXPORT_SYMBOL(mpq_dmx_decoder_fullness_wait);

int mpq_dmx_decoder_fullness_abort(struct dvb_demux_feed *feed)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;

	if (mpq_dmx_is_video_feed(feed)) {
		struct mpq_video_feed_info *feed_data;
		struct dvb_ringbuffer *video_buff;

		spin_lock(&mpq_demux->feed_lock);

		if (feed->priv == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: invalid feed, feed->priv is NULL\n",
				__func__);
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

		feed_data = feed->priv;

		video_buff = &feed_data->video_buffer->raw_data;

		feed_data->fullness_wait_cancel = 1;
		spin_unlock(&mpq_demux->feed_lock);

		wake_up_all(&video_buff->queue);

		return 0;
	}

	/* else */
	MPQ_DVB_ERR_PRINT(
		"%s: Invalid feed type %d\n",
		__func__,
		feed->pes_type);

	return -EINVAL;
}
EXPORT_SYMBOL(mpq_dmx_decoder_fullness_abort);


static inline int mpq_dmx_parse_mandatory_pes_header(
				struct dvb_demux_feed *feed,
				struct mpq_video_feed_info *feed_data,
				struct pes_packet_header *pes_header,
				const u8 *buf,
				u32 *ts_payload_offset,
				int *bytes_avail)
{
	int left_size, copy_len;

	if (feed_data->pes_header_offset < PES_MANDATORY_FIELDS_LEN) {
		left_size =
			PES_MANDATORY_FIELDS_LEN -
			feed_data->pes_header_offset;

		copy_len = (left_size > *bytes_avail) ?
					*bytes_avail :
					left_size;

		memcpy((u8 *)((u8 *)pes_header + feed_data->pes_header_offset),
				(buf + *ts_payload_offset),
				copy_len);

		feed_data->pes_header_offset += copy_len;

		if (left_size > *bytes_avail)
			return -EINVAL;

		/* else - we have beginning of PES header */
		*bytes_avail -= left_size;
		*ts_payload_offset += left_size;

		/* Make sure the PES packet is valid */
		if (mpq_dmx_is_valid_video_pes(pes_header) < 0) {
			/*
			 * Since the new PES header parsing
			 * failed, reset pusi_seen to drop all
			 * data until next PUSI
			 */
			feed->pusi_seen = 0;
			feed_data->pes_header_offset = 0;

			MPQ_DVB_ERR_PRINT(
				"%s: invalid packet\n",
				__func__);

			return -EINVAL;
		}

		feed_data->pes_header_left_bytes =
			pes_header->pes_header_data_length;
	}

	return 0;
}

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

static inline void mpq_dmx_get_pts_dts(struct mpq_video_feed_info *feed_data,
				struct pes_packet_header *pes_header)
{
	struct dmx_pts_dts_info *info = &(feed_data->new_pts_dts_info);

	/* Get PTS/DTS information from PES header */

	if ((pes_header->pts_dts_flag == 2) ||
		(pes_header->pts_dts_flag == 3)) {
		info->pts_exist = 1;

		info->pts =
			((u64)pes_header->pts_1 << 30) |
			((u64)pes_header->pts_2 << 22) |
			((u64)pes_header->pts_3 << 15) |
			((u64)pes_header->pts_4 << 7) |
			(u64)pes_header->pts_5;
	} else {
		info->pts_exist = 0;
		info->pts = 0;
	}

	if (pes_header->pts_dts_flag == 3) {
		info->dts_exist = 1;

		info->dts =
			((u64)pes_header->dts_1 << 30) |
			((u64)pes_header->dts_2 << 22) |
			((u64)pes_header->dts_3 << 15) |
			((u64)pes_header->dts_4 << 7) |
			(u64)pes_header->dts_5;
	} else {
		info->dts_exist = 0;
		info->dts = 0;
	}

	feed_data->new_info_exists = 1;
}

static inline int mpq_dmx_parse_remaining_pes_header(
				struct dvb_demux_feed *feed,
				struct mpq_video_feed_info *feed_data,
				struct pes_packet_header *pes_header,
				const u8 *buf,
				u32 *ts_payload_offset,
				int *bytes_avail)
{
	int left_size, copy_len;

	/* Remainning header bytes that need to be processed? */
	if (!feed_data->pes_header_left_bytes)
		return 0;

	/* Did we capture the PTS value (if exists)? */
	if ((*bytes_avail != 0) &&
		(feed_data->pes_header_offset <
		 (PES_MANDATORY_FIELDS_LEN+5)) &&
		((pes_header->pts_dts_flag == 2) ||
		 (pes_header->pts_dts_flag == 3))) {

		/* 5 more bytes should be there */
		left_size =
			PES_MANDATORY_FIELDS_LEN + 5 -
			feed_data->pes_header_offset;

		copy_len = (left_size > *bytes_avail) ?
					*bytes_avail :
					left_size;

		memcpy((u8 *)((u8 *)pes_header + feed_data->pes_header_offset),
			(buf + *ts_payload_offset),
			copy_len);

		feed_data->pes_header_offset += copy_len;
		feed_data->pes_header_left_bytes -= copy_len;

		if (left_size > *bytes_avail)
			return -EINVAL;

		/* else - we have the PTS */
		*bytes_avail -= copy_len;
		*ts_payload_offset += copy_len;
	}

	/* Did we capture the DTS value (if exist)? */
	if ((*bytes_avail != 0) &&
		(feed_data->pes_header_offset <
		 (PES_MANDATORY_FIELDS_LEN+10)) &&
		(pes_header->pts_dts_flag == 3)) {

		/* 5 more bytes should be there */
		left_size =
			PES_MANDATORY_FIELDS_LEN + 10 -
			feed_data->pes_header_offset;

		copy_len = (left_size > *bytes_avail) ?
					*bytes_avail :
					left_size;

		memcpy((u8 *)((u8 *)pes_header + feed_data->pes_header_offset),
			(buf + *ts_payload_offset),
			copy_len);

		feed_data->pes_header_offset += copy_len;
		feed_data->pes_header_left_bytes -= copy_len;

		if (left_size > *bytes_avail)
			return -EINVAL;

		/* else - we have the DTS */
		*bytes_avail -= copy_len;
		*ts_payload_offset += copy_len;
	}

	/* Any more header bytes?! */
	if (feed_data->pes_header_left_bytes >= *bytes_avail) {
		feed_data->pes_header_left_bytes -= *bytes_avail;
		return -EINVAL;
	}

	/* get PTS/DTS information from PES header to be written later */
	mpq_dmx_get_pts_dts(feed_data, pes_header);

	/* Got PES header, process payload */
	*bytes_avail -= feed_data->pes_header_left_bytes;
	*ts_payload_offset += feed_data->pes_header_left_bytes;
	feed_data->pes_header_left_bytes = 0;

	return 0;
}

static void mpq_dmx_check_continuity(struct mpq_video_feed_info *feed_data,
					int current_continuity,
					int discontinuity_indicator)
{
	const int max_continuity = 0x0F; /* 4 bits in the TS packet header */

	/* sanity check */
	if (unlikely((current_continuity < 0) ||
			(current_continuity > max_continuity))) {
		MPQ_DVB_DBG_PRINT(
			"%s: received invalid continuity counter value %d\n",
					__func__, current_continuity);
		return;
	}

	/* reset last continuity */
	if ((feed_data->last_continuity == -1) ||
		(discontinuity_indicator)) {
		feed_data->last_continuity = current_continuity;
		return;
	}

	/* check for continuity errors */
	if (current_continuity !=
			((feed_data->last_continuity + 1) & max_continuity))
		feed_data->continuity_errs++;

	/* save for next time */
	feed_data->last_continuity = current_continuity;
}

static inline void mpq_dmx_prepare_es_event_data(
			struct mpq_streambuffer_packet_header *packet,
			struct mpq_adapter_video_meta_data *meta_data,
			struct mpq_video_feed_info *feed_data,
			struct mpq_streambuffer *stream_buffer,
			struct dmx_data_ready *data)
{
	size_t len = 0;

	data->data_length = 0;
	data->buf.handle = packet->raw_data_handle;
	/* this has to succeed when called here, after packet was written */
	data->buf.cookie = mpq_streambuffer_pkt_next(stream_buffer,
				feed_data->last_pkt_index, &len);
	data->buf.offset = packet->raw_data_offset;
	data->buf.len = packet->raw_data_len;
	data->buf.pts_exists = meta_data->info.framing.pts_dts_info.pts_exist;
	data->buf.pts = meta_data->info.framing.pts_dts_info.pts;
	data->buf.dts_exists = meta_data->info.framing.pts_dts_info.dts_exist;
	data->buf.dts = meta_data->info.framing.pts_dts_info.dts;
	data->buf.tei_counter = feed_data->tei_errs;
	data->buf.cont_err_counter = feed_data->continuity_errs;
	data->buf.ts_packets_num = feed_data->ts_packets_num;
	data->buf.ts_dropped_bytes = feed_data->ts_dropped_bytes;
	data->status = DMX_OK_DECODER_BUF;

	/* save for next time: */
	feed_data->last_pkt_index = data->buf.cookie;

	/* reset counters */
	feed_data->ts_packets_num = 0;
	feed_data->ts_dropped_bytes = 0;
	feed_data->tei_errs = 0;
	feed_data->continuity_errs = 0;
}

static int mpq_dmx_process_video_packet_framing(
			struct dvb_demux_feed *feed,
			const u8 *buf)
{
	int bytes_avail;
	u32 ts_payload_offset;
	struct mpq_video_feed_info *feed_data;
	const struct ts_packet_header *ts_header;
	struct mpq_streambuffer *stream_buffer;
	struct pes_packet_header *pes_header;
	struct mpq_demux *mpq_demux;

	struct mpq_framing_pattern_lookup_results framing_res;
	struct mpq_streambuffer_packet_header packet;
	struct mpq_adapter_video_meta_data meta_data;
	int bytes_written = 0;
	int bytes_to_write = 0;
	int found_patterns = 0;
	int first_pattern = 0;
	int i;
	int is_video_frame = 0;
	int pending_data_len = 0;
	int ret = 0;
	int discontinuity_indicator = 0;
	struct dmx_data_ready data;

	mpq_demux = feed->demux->priv;

	spin_lock(&mpq_demux->feed_lock);

	feed_data = feed->priv;
	if (unlikely(feed_data == NULL)) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	ts_header = (const struct ts_packet_header *)buf;

	stream_buffer = feed_data->video_buffer;

	pes_header = &feed_data->pes_header;

	/* Make sure this TS packet has a payload and not scrambled */
	if ((ts_header->sync_byte != 0x47) ||
		(ts_header->adaptation_field_control == 0) ||
		(ts_header->adaptation_field_control == 2) ||
		(ts_header->transport_scrambling_control)) {
		/* continue to next packet */
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	if (ts_header->payload_unit_start_indicator) { /* PUSI? */
		if (feed->pusi_seen) { /* Did we see PUSI before? */
			/*
			 * Double check that we are not in middle of
			 * previous PES header parsing.
			 */
			if (feed_data->pes_header_left_bytes != 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: received PUSI"
					"while handling PES header"
					"of previous PES\n",
					__func__);
			}

			feed->peslen = 0;
			feed_data->pes_header_offset = 0;
			feed_data->pes_header_left_bytes =
				PES_MANDATORY_FIELDS_LEN;
		} else {
			feed->pusi_seen = 1;
		}
	}

	/*
	 * Parse PES data only if PUSI was encountered,
	 * otherwise the data is dropped
	 */
	if (!feed->pusi_seen) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0; /* drop and wait for next packets */
	}

	ts_payload_offset = sizeof(struct ts_packet_header);

	/*
	 * Skip adaptation field if exists.
	 * Save discontinuity indicator if exists.
	 */
	if (ts_header->adaptation_field_control == 3) {
		const struct ts_adaptation_field *adaptation_field;
		adaptation_field = (const struct ts_adaptation_field *)
			(buf + ts_payload_offset);
		discontinuity_indicator =
			adaptation_field->discontinuity_indicator;
		ts_payload_offset += buf[ts_payload_offset] + 1;
	}

	/* 188 bytes: the size of a TS packet including the TS packet header */
	bytes_avail = 188 - ts_payload_offset;

	/* Get the mandatory fields of the video PES header */
	if (mpq_dmx_parse_mandatory_pes_header(feed, feed_data,
						pes_header, buf,
						&ts_payload_offset,
						&bytes_avail)) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	if (mpq_dmx_parse_remaining_pes_header(feed, feed_data,
						pes_header, buf,
						&ts_payload_offset,
						&bytes_avail)) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	/*
	 * If we reached here,
	 * then we are now at the PES payload data
	 */
	if (bytes_avail == 0) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	/*
	 * the decoder requires demux to do framing,
	 * so search for the patterns now.
	 */
	found_patterns = mpq_dmx_framing_pattern_search(
				feed_data->patterns,
				feed_data->patterns_num,
				(buf + ts_payload_offset),
				bytes_avail,
				&feed_data->prefix_size,
				&framing_res);

	if (!(feed_data->found_sequence_header_pattern)) {
		for (i = 0; i < found_patterns; i++) {
			if ((framing_res.info[i].type ==
				DMX_FRM_MPEG2_SEQUENCE_HEADER) ||
			    (framing_res.info[i].type ==
				DMX_FRM_H264_SPS) ||
			    (framing_res.info[i].type ==
				DMX_FRM_VC1_SEQUENCE_HEADER)) {

				MPQ_DVB_DBG_PRINT(
					"%s: Found Sequence Pattern, buf %p, i = %d, offset = %d, type = %d\n",
					__func__, buf, i,
					framing_res.info[i].offset,
					framing_res.info[i].type);

				first_pattern = i;
				feed_data->found_sequence_header_pattern = 1;
				ts_payload_offset +=
					framing_res.info[i].offset;
				bytes_avail -= framing_res.info[i].offset;

				if (framing_res.info[i].used_prefix_size) {
					feed_data->first_prefix_size =
						framing_res.info[i].
							used_prefix_size;
				}
				break;
			}
		}
	}

	/*
	 * If decoder requires demux to do framing,
	 * pass data to decoder only after sequence header
	 * or equivalent is found. Otherwise the data is dropped.
	 */
	if (!(feed_data->found_sequence_header_pattern)) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	/* Update error counters based on TS header */
	feed_data->ts_packets_num++;
	feed_data->tei_errs += ts_header->transport_error_indicator;
	mpq_demux->decoder_ts_errors += ts_header->transport_error_indicator;
	mpq_dmx_check_continuity(feed_data,
				ts_header->continuity_counter,
				discontinuity_indicator);

	/* Need to back-up the PTS information of the very first frame */
	if (feed_data->first_pts_dts_copy) {
		for (i = first_pattern; i < found_patterns; i++) {
			is_video_frame = mpq_dmx_is_video_frame(
					feed->indexing_params.standard,
					framing_res.info[i].type);

			if (is_video_frame) {
				mpq_dmx_save_pts_dts(feed_data);
				feed_data->first_pts_dts_copy = 0;
				break;
			}
		}
	}

	/*
	 * write prefix used to find first Sequence pattern, if needed.
	 * feed_data->patterns[0].pattern always contains the Sequence
	 * pattern.
	 */
	if (feed_data->first_prefix_size) {
		if (mpq_streambuffer_data_write(stream_buffer,
					(feed_data->patterns[0].pattern),
					feed_data->first_prefix_size) < 0) {
			mpq_demux->decoder_drop_count +=
				feed_data->first_prefix_size;
			feed_data->ts_dropped_bytes +=
				feed_data->first_prefix_size;
			MPQ_DVB_DBG_PRINT("%s: could not write prefix\n",
				__func__);
		} else {
			MPQ_DVB_DBG_PRINT("%s: Prefix = %d\n",
				__func__, feed_data->first_prefix_size);
			pending_data_len += feed_data->first_prefix_size;
		}
		feed_data->first_prefix_size = 0;
	}

	feed->peslen += bytes_avail;
	pending_data_len += bytes_avail;

	meta_data.packet_type = DMX_FRAMING_INFO_PACKET;
	packet.user_data_len = sizeof(struct mpq_adapter_video_meta_data);

	for (i = first_pattern; i < found_patterns; i++) {
		if (i == first_pattern) {
			if (0 == feed_data->pending_pattern_len) {
				/*
				 * This is the very first pattern, so no
				 * previous pending frame data exists.
				 * Update frame info and skip to the
				 * next frame.
				 */
				feed_data->last_framing_match_type =
					framing_res.info[i].type;
				feed_data->last_pattern_offset =
					framing_res.info[i].offset;
				continue;
			}
			/*
			 * This is the first pattern in this
			 * packet and previous frame from
			 * previous packet is pending for report
			 */
			bytes_to_write = framing_res.info[i].offset;
		} else {
			/*
			 * Previous pending frame is in
			 * the same packet
			 */
			bytes_to_write =
				framing_res.info[i].offset -
				feed_data->last_pattern_offset;
		}

		if (mpq_streambuffer_data_write(
			stream_buffer,
			(buf + ts_payload_offset + bytes_written),
			bytes_to_write) < 0) {
			mpq_demux->decoder_drop_count += bytes_to_write;
			feed_data->ts_dropped_bytes += bytes_to_write;
			MPQ_DVB_DBG_PRINT(
				"%s: Couldn't write %d bytes to data buffer\n",
				__func__, bytes_to_write);
		} else {
			bytes_written += bytes_to_write;
			pending_data_len -= bytes_to_write;
			feed_data->pending_pattern_len += bytes_to_write;
		}

		is_video_frame = mpq_dmx_is_video_frame(
				feed->indexing_params.standard,
				feed_data->last_framing_match_type);
		if (is_video_frame == 1) {
			struct timespec curr_time, delta_time;
			u64 delta_time_ms;

			mpq_dmx_write_pts_dts(feed_data,
				&(meta_data.info.framing.pts_dts_info));
			mpq_dmx_save_pts_dts(feed_data);

			packet.raw_data_len = feed_data->pending_pattern_len;
			packet.raw_data_offset = feed_data->frame_offset;
			meta_data.info.framing.pattern_type =
				feed_data->last_framing_match_type;

			mpq_streambuffer_get_buffer_handle(
				stream_buffer,
				0,	/* current write buffer handle */
				&packet.raw_data_handle);

			curr_time = current_kernel_time();
			if (likely(mpq_demux->decoder_out_count)) {
				/* calculate time-delta between frame */
				delta_time = timespec_sub(curr_time,
				mpq_demux->decoder_out_last_time);

				delta_time_ms =
				  ((u64)delta_time.tv_sec * MSEC_PER_SEC)
				  + delta_time.tv_nsec / NSEC_PER_MSEC;

				mpq_demux->decoder_out_interval_sum +=
				  (u32)delta_time_ms;

				mpq_demux->
				  decoder_out_interval_average =
				  mpq_demux->decoder_out_interval_sum /
				  mpq_demux->decoder_out_count;

				if (delta_time_ms >
				    mpq_demux->decoder_out_interval_max)
					mpq_demux->
						decoder_out_interval_max =
						delta_time_ms;
			}

			mpq_demux->decoder_out_last_time = curr_time;
			mpq_demux->decoder_out_count++;

			/*
			 * writing meta-data that includes
			 * the framing information
			 */
			if (mpq_streambuffer_pkt_write(stream_buffer,
				&packet,
				(u8 *)&meta_data) < 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: "
					"Couldn't write packet. "
					"Should never happen\n",
					__func__);
			}

			if (generate_es_events) {
				mpq_dmx_prepare_es_event_data(
					&packet, &meta_data, feed_data,
					stream_buffer, &data);

				feed->data_ready_cb.ts(&feed->feed.ts, &data);
			}

			feed_data->pending_pattern_len = 0;
			mpq_streambuffer_get_data_rw_offset(
				feed_data->video_buffer,
				NULL,
				&feed_data->frame_offset);
		}

		/* save the last match for next time */
		feed_data->last_framing_match_type =
			framing_res.info[i].type;
		feed_data->last_pattern_offset =
			framing_res.info[i].offset;
	}

	if (pending_data_len) {
		ret = mpq_streambuffer_data_write(
			stream_buffer,
			(buf + ts_payload_offset + bytes_written),
			pending_data_len);
		if (ret < 0) {
			mpq_demux->decoder_drop_count += pending_data_len;
			feed_data->ts_dropped_bytes += pending_data_len;
			MPQ_DVB_DBG_PRINT(
				"%s: Couldn't write %d bytes to data buffer\n",
				__func__, pending_data_len);
		} else {
			feed_data->pending_pattern_len += pending_data_len;
		}
	}

	spin_unlock(&mpq_demux->feed_lock);
	return 0;
}

static int mpq_dmx_process_video_packet_no_framing(
			struct dvb_demux_feed *feed,
			const u8 *buf)
{
	int bytes_avail;
	u32 ts_payload_offset;
	struct mpq_video_feed_info *feed_data;
	const struct ts_packet_header *ts_header;
	struct mpq_streambuffer *stream_buffer;
	struct pes_packet_header *pes_header;
	struct mpq_demux *mpq_demux;
	int discontinuity_indicator = 0;
	struct dmx_data_ready data;

	mpq_demux = feed->demux->priv;

	spin_lock(&mpq_demux->feed_lock);

	feed_data = feed->priv;
	if (unlikely(feed_data == NULL)) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	ts_header = (const struct ts_packet_header *)buf;

	stream_buffer = feed_data->video_buffer;

	pes_header = &feed_data->pes_header;

	/* Make sure this TS packet has a payload and not scrambled */
	if ((ts_header->sync_byte != 0x47) ||
		(ts_header->adaptation_field_control == 0) ||
		(ts_header->adaptation_field_control == 2) ||
		(ts_header->transport_scrambling_control)) {
		/* continue to next packet */
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	if (ts_header->payload_unit_start_indicator) { /* PUSI? */
		if (feed->pusi_seen) { /* Did we see PUSI before? */
			struct mpq_streambuffer_packet_header packet;
			struct mpq_adapter_video_meta_data meta_data;

			/*
			 * Close previous PES.
			 * Push new packet to the meta-data buffer.
			 * Double check that we are not in middle of
			 * previous PES header parsing.
			 */

			if (0 == feed_data->pes_header_left_bytes) {
				packet.raw_data_len = feed->peslen;
				mpq_streambuffer_get_buffer_handle(
					stream_buffer,
					0, /* current write buffer handle */
					&packet.raw_data_handle);
				packet.raw_data_offset =
					feed_data->frame_offset;
				packet.user_data_len =
					sizeof(struct
						mpq_adapter_video_meta_data);

				mpq_dmx_write_pts_dts(feed_data,
					&(meta_data.info.pes.pts_dts_info));
				mpq_dmx_save_pts_dts(feed_data);

				meta_data.packet_type = DMX_PES_PACKET;

				if (mpq_streambuffer_pkt_write(
						stream_buffer,
						&packet,
						(u8 *)&meta_data) < 0)
					MPQ_DVB_ERR_PRINT(
						"%s: "
						"Couldn't write packet. "
						"Should never happen\n",
						__func__);

				/* Save write offset where new PES will begin */
				mpq_streambuffer_get_data_rw_offset(
					stream_buffer,
					NULL,
					&feed_data->frame_offset);

				if (generate_es_events) {
					mpq_dmx_prepare_es_event_data(
						&packet, &meta_data,
						feed_data,
						stream_buffer, &data);

					feed->data_ready_cb.ts(
						&feed->feed.ts, &data);
				}
			} else {
				MPQ_DVB_ERR_PRINT(
					"%s: received PUSI"
					"while handling PES header"
					"of previous PES\n",
					__func__);
			}

			/* Reset PES info */
			feed_data->pes_payload_address =
				(u32)stream_buffer->raw_data.data +
				stream_buffer->raw_data.pwrite;

			feed->peslen = 0;
			feed_data->pes_header_offset = 0;
			feed_data->pes_header_left_bytes =
				PES_MANDATORY_FIELDS_LEN;
		} else {
			feed->pusi_seen = 1;
		}
	}

	/*
	 * Parse PES data only if PUSI was encountered,
	 * otherwise the data is dropped
	 */
	if (!feed->pusi_seen) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0; /* drop and wait for next packets */
	}

	ts_payload_offset = sizeof(struct ts_packet_header);

	/*
	 * Skip adaptation field if exists.
	 * Save discontinuity indicator if exists.
	 */
	if (ts_header->adaptation_field_control == 3) {
		const struct ts_adaptation_field *adaptation_field;
		adaptation_field = (const struct ts_adaptation_field *)
			(buf + ts_payload_offset);
		discontinuity_indicator =
			adaptation_field->discontinuity_indicator;
		ts_payload_offset += buf[ts_payload_offset] + 1;
	}

	/* 188 bytes: size of a TS packet including the TS packet header */
	bytes_avail = 188 - ts_payload_offset;

	/* Get the mandatory fields of the video PES header */
	if (mpq_dmx_parse_mandatory_pes_header(feed, feed_data,
						pes_header, buf,
						&ts_payload_offset,
						&bytes_avail)) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	if (mpq_dmx_parse_remaining_pes_header(feed, feed_data,
						pes_header, buf,
						&ts_payload_offset,
						&bytes_avail)) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	/*
	 * If we reached here,
	 * then we are now at the PES payload data
	 */
	if (bytes_avail == 0) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	/*
	 * Need to back-up the PTS information
	 * of the very first PES
	 */
	if (feed_data->first_pts_dts_copy) {
		mpq_dmx_save_pts_dts(feed_data);
		feed_data->first_pts_dts_copy = 0;
	}

	/* Update error counters based on TS header */
	feed_data->ts_packets_num++;
	feed_data->tei_errs += ts_header->transport_error_indicator;
	mpq_demux->decoder_ts_errors += ts_header->transport_error_indicator;
	mpq_dmx_check_continuity(feed_data,
				ts_header->continuity_counter,
				discontinuity_indicator);

	if (mpq_streambuffer_data_write(
				stream_buffer,
				buf+ts_payload_offset,
				bytes_avail) < 0) {
		mpq_demux->decoder_drop_count += bytes_avail;
		feed_data->ts_dropped_bytes += bytes_avail;
	} else {
		feed->peslen += bytes_avail;
	}

	spin_unlock(&mpq_demux->feed_lock);

	return 0;
}

int mpq_dmx_decoder_buffer_status(struct dvb_demux_feed *feed,
		struct dmx_buffer_status *dmx_buffer_status)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_video_feed_info *feed_data;
	struct mpq_streambuffer *video_buff;

	if (!mpq_dmx_is_video_feed(feed)) {
		MPQ_DVB_ERR_PRINT(
			"%s: Invalid feed type %d\n",
			__func__,
			feed->pes_type);
		return -EINVAL;
	}

	spin_lock(&mpq_demux->feed_lock);

	if (feed->priv == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid feed, feed->priv is NULL\n",
			__func__);
		spin_unlock(&mpq_demux->feed_lock);
		return -EINVAL;
	}

	feed_data = feed->priv;
	video_buff = feed_data->video_buffer;
	if (!video_buff) {
		spin_unlock(&mpq_demux->feed_lock);
		return -EINVAL;
	}

	dmx_buffer_status->error = video_buff->raw_data.error;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR == video_buff->mode) {
		dmx_buffer_status->fullness =
			video_buff->buffers[0].size *
			video_buff->pending_buffers_count;
		dmx_buffer_status->free_bytes =
			video_buff->buffers[0].size *
			(video_buff->buffers_num -
			video_buff->pending_buffers_count);
		dmx_buffer_status->size =
			video_buff->buffers[0].size *
			video_buff->buffers_num;
	} else {
		dmx_buffer_status->fullness =
			mpq_streambuffer_data_avail(video_buff);
		dmx_buffer_status->free_bytes =
			mpq_streambuffer_data_free(video_buff);
		dmx_buffer_status->size = video_buff->buffers[0].size;
	}

	mpq_streambuffer_get_data_rw_offset(
		video_buff,
		&dmx_buffer_status->read_offset,
		&dmx_buffer_status->write_offset);

	spin_unlock(&mpq_demux->feed_lock);

	return 0;
}
EXPORT_SYMBOL(mpq_dmx_decoder_buffer_status);

int mpq_dmx_process_video_packet(
			struct dvb_demux_feed *feed,
			const u8 *buf)
{
	if (mpq_dmx_info.decoder_framing)
		return mpq_dmx_process_video_packet_no_framing(feed, buf);
	else
		return mpq_dmx_process_video_packet_framing(feed, buf);
}
EXPORT_SYMBOL(mpq_dmx_process_video_packet);

int mpq_dmx_process_pcr_packet(
			struct dvb_demux_feed *feed,
			const u8 *buf)
{
	u64 pcr;
	u64 stc;
	struct dmx_data_ready data;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	const struct ts_packet_header *ts_header;
	const struct ts_adaptation_field *adaptation_field;

	ts_header = (const struct ts_packet_header *)buf;

	/* Make sure this TS packet has a adaptation field */
	if ((ts_header->sync_byte != 0x47) ||
		(ts_header->adaptation_field_control == 0) ||
		(ts_header->adaptation_field_control == 1) ||
		ts_header->transport_error_indicator)
		return 0;

	adaptation_field = (const struct ts_adaptation_field *)
			(buf + sizeof(struct ts_packet_header));

	if ((!adaptation_field->adaptation_field_length) ||
		(!adaptation_field->PCR_flag))
		return 0; /* 0 adaptation field or no PCR */

	pcr = ((u64)adaptation_field->program_clock_reference_base_1) << 25;
	pcr += ((u64)adaptation_field->program_clock_reference_base_2) << 17;
	pcr += ((u64)adaptation_field->program_clock_reference_base_3) << 9;
	pcr += ((u64)adaptation_field->program_clock_reference_base_4) << 1;
	pcr += adaptation_field->program_clock_reference_base_5;
	pcr *= 300;
	pcr +=
		(((u64)adaptation_field->program_clock_reference_ext_1) << 8) +
		adaptation_field->program_clock_reference_ext_2;

	/*
	 * When we play from front-end, we configure HW
	 * to output the extra timestamp, if we are playing
	 * from DVR, we don't have a timestamp if the packet
	 * format is not 192-tail.
	 */
	if ((mpq_demux->source >= DMX_SOURCE_DVR0) &&
		(mpq_demux->demux.tsp_format != DMX_TSP_FORMAT_192_TAIL)) {
		stc = 0;
	} else {
		stc = buf[190] << 16;
		stc += buf[189] << 8;
		stc += buf[188];
		stc *= 256; /* convert from 105.47 KHZ to 27MHz */
	}

	data.data_length = 0;
	data.pcr.pcr = pcr;
	data.pcr.stc = stc;
	data.pcr.disc_indicator_set = adaptation_field->discontinuity_indicator;
	data.status = DMX_OK_PCR;
	feed->data_ready_cb.ts(&feed->feed.ts, &data);

	return 0;
}
EXPORT_SYMBOL(mpq_dmx_process_pcr_packet);
