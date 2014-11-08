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

#ifndef __Q6CORE_H__
#define __Q6CORE_H__
#include <mach/qdsp6v2/apr.h>
#include <mach/ocmem.h>


#define AVCS_CMD_GET_LOW_POWER_SEGMENTS_INFO              0x00012903

struct avcs_cmd_get_low_power_segments_info {
	struct apr_hdr hdr;
} __packed;


#define AVCS_CMDRSP_GET_LOW_POWER_SEGMENTS_INFO           0x00012904

#define AVCS_CMD_ADSP_EVENT_GET_STATE		0x0001290C
#define AVCS_CMDRSP_ADSP_EVENT_GET_STATE	0x0001290D

/* @brief AVCS_CMDRSP_GET_LOW_POWER_SEGMENTS_INFO payload
 * structure. Payload for this event comprises one instance of
 * avcs_cmd_rsp_get_low_power_segments_info_t, followed
 * immediately by num_segments number of instances of the
 * avcs_mem_segment_t structure.
 */

/* Types of Low Power Memory Segments. */
#define READ_ONLY_SEGMENT      1
/*< Read Only memory segment. */
#define READ_WRITE_SEGMENT     2
/*< Read Write memory segment. */
/*Category indicates whether audio/os/sensor segments. */
#define AUDIO_SEGMENT          1
/*< Audio memory segment. */
#define OS_SEGMENT             2
/*< QDSP6's OS memory segment. */

/* @brief Payload structure for AVS low power memory segment
 *  structure.
 */
struct avcs_mem_segment_t {
	uint16_t              type;
/*< Indicates which type of memory this segment is.
 *Allowed values: READ_ONLY_SEGMENT or READ_WRITE_SEGMENT only.
 */
	uint16_t              category;
/*< Indicates whether audio or OS segments.
 *Allowed values: AUDIO_SEGMENT or OS_SEGMENT only.
 */
	uint32_t              size;
/*< Size (in bytes) of this segment.
 * Will be a non-zero value.
 */
	uint32_t              start_address_lsw;
/*< Lower 32 bits of the 64-bit physical start address
 * of this segment.
 */
	uint32_t              start_address_msw;
/*< Upper 32 bits of the 64-bit physical start address
 * of this segment.
 */
};

struct avcs_cmd_rsp_get_low_power_segments_info_t {
	uint32_t              num_segments;
/*< Number of segments in this response.
 * 0: there are no known sections that should be mapped
 * from DDR to OCMEM.
 * >0: the number of memory segments in the following list.
 */

	uint32_t              bandwidth;
/*< Required OCMEM read/write bandwidth (in bytes per second)
 * if OCMEM is granted.
 * 0 if num_segments = 0
 * >0 if num_segments > 0.
 */
	struct avcs_mem_segment_t mem_segment[OCMEM_MAX_CHUNKS];
};


int core_get_low_power_segments(
			struct avcs_cmd_rsp_get_low_power_segments_info_t **);
bool q6core_is_adsp_ready(void);

#define ADSP_CMD_SET_DOLBY_MANUFACTURER_ID 0x00012918

struct adsp_dolby_manufacturer_id {
	struct apr_hdr hdr;
	int manufacturer_id;
};

uint32_t core_set_dolby_manufacturer_id(int manufacturer_id);

#endif /* __Q6CORE_H__ */
