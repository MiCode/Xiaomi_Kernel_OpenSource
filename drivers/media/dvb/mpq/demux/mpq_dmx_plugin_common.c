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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
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

/*
 * PCR/STC information length saved in ring-buffer.
 * PCR / STC are saved in ring-buffer in the following form:
 * <8 bit flags><64 bits of STC> <64bits of PCR>
 * STC and PCR values are in 27MHz.
 * The current flags that are defined:
 * 0x00000001: discontinuity_indicator
 */
#define PCR_STC_LEN					17


/* Number of demux devices, has default of linux configuration */
static int mpq_demux_device_num = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
module_param(mpq_demux_device_num, int, S_IRUGO);

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
						"%s: Found matching pattern"
						"using prefix of size %d\n",
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
	mpq_demux->hw_notification_rate = 0;
	mpq_demux->hw_notification_size = 0;
	mpq_demux->decoder_tsp_drop_count = 0;

	if (mpq_demux->demux.debugfs_demux_dir != NULL) {
		debugfs_create_u32(
			"hw_notification_rate",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.debugfs_demux_dir,
			&mpq_demux->hw_notification_rate);

		debugfs_create_u32(
			"hw_notification_count",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.debugfs_demux_dir,
			&mpq_demux->hw_notification_count);

		debugfs_create_u32(
			"hw_notification_size",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.debugfs_demux_dir,
			&mpq_demux->hw_notification_size);

		debugfs_create_u32(
			"decoder_tsp_drop_count",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.debugfs_demux_dir,
			&mpq_demux->decoder_tsp_drop_count);
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

		delta_time_ms = (u64)timespec_to_ns(&delta_time);
		delta_time_ms = div64_u64(delta_time_ms, 1000000); /* ns->ms */

		mpq_demux->hw_notification_rate = delta_time_ms;
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
		msm_ion_client_create(UINT_MAX, "demux client");

	if (IS_ERR_OR_NULL(mpq_dmx_info.ion_client)) {
		MPQ_DVB_ERR_PRINT(
				"%s: msm_ion_client_create\n",
				__func__);

		result = PTR_ERR(mpq_dmx_info.ion_client);
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
			mpq_demux = mpq_dmx_info.devices+i;

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


int mpq_dmx_init_video_feed(struct dvb_demux_feed *feed)
{
	int ret;
	void *packet_buffer;
	void *payload_buffer;
	struct mpq_video_feed_info *feed_data;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_streambuffer *stream_buffer;
	int actual_buffer_size;

	/* Allocate memory for private feed data */
	feed_data = vmalloc(sizeof(struct mpq_video_feed_info));

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

	/* Allocate packet buffer holding the meta-data */
	packet_buffer = vmalloc(VIDEO_META_DATA_BUFFER_SIZE);

	if (packet_buffer == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to allocate packets buffer\n",
			__func__);

		ret = -ENOMEM;
		goto init_failed_free_priv_data;
	}

	/*
	 * Allocate payload buffer through ION.
	 * TODO: for scrambling support, need to check if the
	 * stream is scrambled and allocate the buffer with secure
	 * flag set.
	 */

	actual_buffer_size = feed->buffer_size;

	actual_buffer_size += (SZ_4K - 1);
	actual_buffer_size &= ~(SZ_4K - 1);

	feed_data->payload_buff_handle =
		ion_alloc(mpq_demux->ion_client,
				  actual_buffer_size,
				  SZ_4K,
				  ION_HEAP(ION_CP_MM_HEAP_ID));

	if (IS_ERR_OR_NULL(feed_data->payload_buff_handle)) {
		ret = PTR_ERR(feed_data->payload_buff_handle);

		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to allocate payload buffer %d\n",
			__func__,
			ret);

		goto init_failed_free_packet_buffer;
	}

	payload_buffer =
		ion_map_kernel(mpq_demux->ion_client,
					   feed_data->payload_buff_handle,
					   0);

	if (IS_ERR_OR_NULL(payload_buffer)) {
		ret = PTR_ERR(payload_buffer);

		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to map payload buffer %d\n",
			__func__,
			ret);

		goto init_failed_free_payload_buffer;
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
		goto init_failed_unmap_payload_buffer;
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
		goto init_failed_unmap_payload_buffer;
	}

	feed_data->video_buffer =
		&mpq_dmx_info.decoder_buffers[feed_data->stream_interface];

	mpq_streambuffer_init(
			feed_data->video_buffer,
			payload_buffer,
			actual_buffer_size,
			packet_buffer,
			VIDEO_META_DATA_BUFFER_SIZE);

	ret =
		mpq_adapter_register_stream_if(
			feed_data->stream_interface,
			feed_data->video_buffer);

	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_adapter_register_stream_if failed, "
			"err = %d\n",
			__func__,
			ret);
		goto init_failed_unmap_payload_buffer;
	}

	feed->buffer_size = actual_buffer_size;
	feed_data->pes_payload_address =
		(u32)feed_data->video_buffer->raw_data.data;

	feed_data->pes_header_left_bytes = PES_MANDATORY_FIELDS_LEN;
	feed_data->pes_header_offset = 0;
	feed->pusi_seen = 0;
	feed->peslen = 0;
	feed_data->fullness_wait_cancel = 0;
	feed_data->last_framing_match_address = 0;
	feed_data->last_framing_match_type = DMX_FRM_UNKNOWN;
	feed_data->found_sequence_header_pattern = 0;
	memset(&feed_data->prefix_size, 0,
			sizeof(struct mpq_framing_prefix_size_masks));
	feed_data->first_pattern_offset = 0;
	feed_data->first_prefix_size = 0;
	feed_data->write_pts_dts = 0;

	spin_lock(&mpq_demux->feed_lock);
	feed->priv = (void *)feed_data;
	spin_unlock(&mpq_demux->feed_lock);

	return 0;

init_failed_unmap_payload_buffer:
	ion_unmap_kernel(mpq_demux->ion_client,
					 feed_data->payload_buff_handle);
init_failed_free_payload_buffer:
	ion_free(mpq_demux->ion_client,
			feed_data->payload_buff_handle);
init_failed_free_packet_buffer:
	vfree(packet_buffer);
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

	mpq_adapter_unregister_stream_if(
		feed_data->stream_interface);

	vfree(feed_data->video_buffer->packet_data.data);

	ion_unmap_kernel(mpq_demux->ion_client,
					 feed_data->payload_buff_handle);

	ion_free(mpq_demux->ion_client,
			 feed_data->payload_buff_handle);

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

int mpq_dmx_decoder_fullness_wait(
		struct dvb_demux_feed *feed,
		size_t required_space)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;

	if (mpq_dmx_is_video_feed(feed)) {
		int ret;
		struct mpq_video_feed_info *feed_data;
		struct dvb_ringbuffer *video_buff;

		spin_lock(&mpq_demux->feed_lock);

		if (feed->priv == NULL) {
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

		feed_data = feed->priv;
		video_buff = &feed_data->video_buffer->raw_data;

		ret = 0;
		if ((feed_data != NULL) &&
			(!feed_data->fullness_wait_cancel) &&
			(dvb_ringbuffer_free(video_buff) < required_space)) {
			DEFINE_WAIT(__wait);
			for (;;) {
				prepare_to_wait(
					&video_buff->queue,
					&__wait,
					TASK_INTERRUPTIBLE);

				if ((feed->priv == NULL) ||
					(feed_data->fullness_wait_cancel) ||
					(dvb_ringbuffer_free(video_buff) >=
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
			finish_wait(&video_buff->queue, &__wait);
		}

		if (ret < 0) {
			spin_unlock(&mpq_demux->feed_lock);
			return ret;
		}

		if ((feed->priv == NULL) ||
			(feed_data->fullness_wait_cancel)) {
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

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
		feed_data->write_pts_dts = 1;
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
		feed_data->write_pts_dts = 1;
	}

	/* Any more header bytes?! */
	if (feed_data->pes_header_left_bytes >= *bytes_avail) {
		feed_data->pes_header_left_bytes -= *bytes_avail;
		return -EINVAL;
	}

	/* Got PES header, process payload */
	*bytes_avail -= feed_data->pes_header_left_bytes;
	*ts_payload_offset += feed_data->pes_header_left_bytes;
	feed_data->pes_header_left_bytes = 0;

	return 0;
}

static inline void mpq_dmx_get_pts_dts(struct mpq_video_feed_info *feed_data,
				struct pes_packet_header *pes_header,
				struct mpq_adapter_video_meta_data *meta_data,
				enum dmx_packet_type packet_type)
{
	struct dmx_pts_dts_info *info;

	if (packet_type == DMX_PES_PACKET)
		info = &(meta_data->info.pes.pts_dts_info);
	else
		info = &(meta_data->info.framing.pts_dts_info);

	if (feed_data->write_pts_dts) {
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
	} else {
		info->pts_exist = 0;
		info->dts_exist = 0;
	}
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
	int found_patterns = 0;
	int first_pattern = 0;
	int i;
	u32 pattern_addr = 0;
	int is_video_frame = 0;

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

	/* MPQ_DVB_DBG_PRINT("TS packet: %X %X %X %X %X%X %X %X %X\n",
		ts_header->sync_byte,
		ts_header->transport_error_indicator,
		ts_header->payload_unit_start_indicator,
		ts_header->transport_priority,
		ts_header->pid_msb,
		ts_header->pid_lsb,
		ts_header->transport_scrambling_control,
		ts_header->adaptation_field_control,
		ts_header->continuity_counter); */

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
			feed_data->write_pts_dts = 0;
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

	/* Skip adaptation field if exists */
	if (ts_header->adaptation_field_control == 3)
		ts_payload_offset += buf[ts_payload_offset] + 1;

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
					"%s: Found Sequence Pattern, buf %p, "
					"i = %d, offset = %d, type = %d\n",
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
				/*
				 * if this is the first pattern we write,
				 * no need to take offset into account since we
				 * dropped all data before it (so effectively
				 * offset is 0).
				 * we save the first pattern offset and take
				 * it into consideration for the rest of the
				 * patterns found in this buffer.
				 */
				feed_data->first_pattern_offset =
					framing_res.info[i].offset;
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

	/*
	 * write prefix used to find first Sequence pattern, if needed.
	 * feed_data->patterns[0].pattern always contains the Sequence
	 * pattern.
	 */
	if (feed_data->first_prefix_size) {
		if (mpq_streambuffer_data_write(stream_buffer,
					(feed_data->patterns[0].pattern),
					feed_data->first_prefix_size) < 0) {
			mpq_demux->decoder_tsp_drop_count++;
			spin_unlock(&mpq_demux->feed_lock);
			return 0;
		}
		feed_data->first_prefix_size = 0;
	}
	/* write data to payload buffer */
	if (mpq_streambuffer_data_write(stream_buffer,
					(buf + ts_payload_offset),
					bytes_avail) < 0) {
		mpq_demux->decoder_tsp_drop_count++;
	} else {
		struct mpq_streambuffer_packet_header packet;
		struct mpq_adapter_video_meta_data meta_data;

		feed->peslen += bytes_avail;

		meta_data.packet_type = DMX_FRAMING_INFO_PACKET;
		packet.user_data_len =
				sizeof(struct mpq_adapter_video_meta_data);

		for (i = first_pattern; i < found_patterns; i++) {
			if (feed_data->last_framing_match_address) {
				is_video_frame = mpq_dmx_is_video_frame(
					feed->indexing_params.standard,
					feed_data->last_framing_match_type);
				if (is_video_frame == 1) {
					mpq_dmx_get_pts_dts(feed_data,
						pes_header,
						&meta_data,
						DMX_FRAMING_INFO_PACKET);
				} else {
					meta_data.info.framing.
						pts_dts_info.pts_exist = 0;
					meta_data.info.framing.
						pts_dts_info.dts_exist = 0;
				}
				/*
				 * writing meta-data that includes
				 * framing information
				 */
				meta_data.info.framing.pattern_type =
					feed_data->last_framing_match_type;
				packet.raw_data_addr =
					feed_data->last_framing_match_address;

				pattern_addr = feed_data->pes_payload_address +
					framing_res.info[i].offset -
					framing_res.info[i].used_prefix_size;

				if ((pattern_addr -
					feed_data->first_pattern_offset) <
					feed_data->last_framing_match_address) {
					/* wraparound case */
					packet.raw_data_len =
						(pattern_addr -
						feed_data->
						   last_framing_match_address +
						stream_buffer->raw_data.size) -
						feed_data->first_pattern_offset;
				} else {
					packet.raw_data_len =
					  pattern_addr -
					  feed_data->
						last_framing_match_address -
					  feed_data->first_pattern_offset;
				}

				MPQ_DVB_DBG_PRINT("Writing Packet: "
					"addr = 0x%X, len = %d, type = %d, "
					"isPts = %d, isDts = %d\n",
					packet.raw_data_addr,
					packet.raw_data_len,
					meta_data.info.framing.pattern_type,
					meta_data.info.framing.
						pts_dts_info.pts_exist,
					meta_data.info.framing.
						pts_dts_info.dts_exist);

				if (mpq_streambuffer_pkt_write(stream_buffer,
						&packet,
						(u8 *)&meta_data) < 0) {
							MPQ_DVB_ERR_PRINT(
								"%s: "
								"Couldn't write packet. "
								"Should never happen\n",
								__func__);
				} else {
					if (is_video_frame == 1)
						feed_data->write_pts_dts = 0;
				}
			}

			/* save the last match for next time */
			feed_data->last_framing_match_type =
					framing_res.info[i].type;

			feed_data->last_framing_match_address =
				(feed_data->pes_payload_address +
				framing_res.info[i].offset -
				framing_res.info[i].used_prefix_size -
				feed_data->first_pattern_offset);
		}
		/*
		 * the first pattern offset is needed only for the group of
		 * patterns that are found and written with the first pattern.
		 */
		feed_data->first_pattern_offset = 0;

		feed_data->pes_payload_address =
			(u32)stream_buffer->raw_data.data +
			stream_buffer->raw_data.pwrite;
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

	mpq_demux = feed->demux->priv;

	spin_lock(&mpq_demux->feed_lock);

	feed_data = feed->priv;
	if (unlikely(feed_data == NULL)) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	ts_header = (const struct ts_packet_header *)buf;

	stream_buffer =	feed_data->video_buffer;

	pes_header = &feed_data->pes_header;

	/* MPQ_DVB_DBG_PRINT("TS packet: %X %X %X %X %X%X %X %X %X\n",
		ts_header->sync_byte,
		ts_header->transport_error_indicator,
		ts_header->payload_unit_start_indicator,
		ts_header->transport_priority,
		ts_header->pid_msb,
		ts_header->pid_lsb,
		ts_header->transport_scrambling_control,
		ts_header->adaptation_field_control,
		ts_header->continuity_counter); */

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
				packet.raw_data_addr =
					feed_data->pes_payload_address;

				packet.raw_data_len = feed->peslen;

				packet.user_data_len =
					sizeof(struct
						mpq_adapter_video_meta_data);

				mpq_dmx_get_pts_dts(feed_data, pes_header,
							&meta_data,
							DMX_PES_PACKET);

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
				else
					feed_data->write_pts_dts = 0;
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

	/* Skip adaptation field if exists */
	if (ts_header->adaptation_field_control == 3)
		ts_payload_offset += buf[ts_payload_offset] + 1;

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

	if (mpq_streambuffer_data_write(
				stream_buffer,
				buf+ts_payload_offset,
				bytes_avail) < 0)
		mpq_demux->decoder_tsp_drop_count++;
	else
		feed->peslen += bytes_avail;

	spin_unlock(&mpq_demux->feed_lock);

	return 0;
}

int mpq_dmx_decoder_buffer_status(struct dvb_demux_feed *feed,
		struct dmx_buffer_status *dmx_buffer_status)
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

		dmx_buffer_status->error = video_buff->error;
		dmx_buffer_status->fullness = dvb_ringbuffer_avail(video_buff);
		dmx_buffer_status->free_bytes = dvb_ringbuffer_free(video_buff);
		dmx_buffer_status->read_offset = video_buff->pread;
		dmx_buffer_status->write_offset = video_buff->pwrite;
		dmx_buffer_status->size = video_buff->size;

		spin_unlock(&mpq_demux->feed_lock);

		return 0;
	}

	/* else */
	MPQ_DVB_ERR_PRINT(
		"%s: Invalid feed type %d\n",
		__func__,
		feed->pes_type);

	return -EINVAL;
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
	int i;
	u64 pcr;
	u64 stc;
	u8 output[PCR_STC_LEN];
	struct mpq_demux *mpq_demux = feed->demux->priv;
	const struct ts_packet_header *ts_header;
	const struct ts_adaptation_field *adaptation_field;

	/*
	 * When we play from front-end, we configure HW
	 * to output the extra timestamp, if we are playing
	 * from DVR, make sure the format is 192 packet.
	 */
	if ((mpq_demux->source >= DMX_SOURCE_DVR0) &&
		(mpq_demux->demux.tsp_format != DMX_TSP_FORMAT_192_TAIL)) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid packet format %d for PCR extraction\n",
			__func__,
			mpq_demux->demux.tsp_format);

		 return -EINVAL;
	}

	ts_header = (const struct ts_packet_header *)buf;

	/* Make sure this TS packet has a adaptation field */
	if ((ts_header->sync_byte != 0x47) ||
		(ts_header->adaptation_field_control == 0) ||
		(ts_header->adaptation_field_control == 1)) {
		return 0;
	}

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

	stc = buf[190] << 16;
	stc += buf[189] << 8;
	stc += buf[188];
	stc *= 256; /* convert from 105.47 KHZ to 27MHz */

	output[0] = adaptation_field->discontinuity_indicator;

	for (i = 1; i <= 8; i++)
		output[i] = (stc >> ((8-i) << 3)) & 0xFF;

	for (i = 9; i <= 16; i++)
		output[i] = (pcr >> ((16-i) << 3)) & 0xFF;

	feed->cb.ts(output, PCR_STC_LEN,
				NULL, 0,
				&feed->feed.ts, DMX_OK);
	return 0;
}
EXPORT_SYMBOL(mpq_dmx_process_pcr_packet);

