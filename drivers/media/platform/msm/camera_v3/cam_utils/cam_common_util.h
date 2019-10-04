/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_COMMON_UTIL_H_
#define _CAM_COMMON_UTIL_H_

#include <linux/types.h>
#include <linux/kernel.h>

#define CAM_BITS_MASK_SHIFT(x, mask, shift) (((x) & (mask)) >> shift)

#define PTR_TO_U64(ptr) ((uint64_t)(uintptr_t)ptr)
#define U64_TO_PTR(ptr) ((void *)(uintptr_t)ptr)

/**
 * cam_common_util_get_string_index()
 *
 * @brief                  Match the string from list of strings to return
 *                         matching index
 *
 * @strings:               Pointer to list of strings
 * @num_strings:           Number of strings in 'strings'
 * @matching_string:       String to match
 * @index:                 Pointer to index to return matching index
 *
 * @return:                0 for success
 *                         -EINVAL for Fail
 */
int cam_common_util_get_string_index(const char **strings,
	uint32_t num_strings, char *matching_string, uint32_t *index);

/**
 * cam_common_util_remove_duplicate_arr()
 *
 * @brief                  Move all the unique integers to the start of
 *                         the array and return the number of unique integers
 *
 * @array:                 Pointer to the first integer of array
 * @num:                   Number of elements in array
 *
 * @return:                Number of unique integers in array
 */
uint32_t cam_common_util_remove_duplicate_arr(int32_t *array,
	uint32_t num);

/**
 * cam_common_util_get_time_diff()
 *
 * @brief                  Get the time difference between 2 timestamps in usecs
 *
 * @t1:                    Pointer to the later time
 * @t2:                    Pointer to the prev time
 *
 * @return:                differnce in usecs
 */
uint64_t cam_common_util_get_time_diff(struct timeval *t1, struct timeval *t2);

/**
 * cam_comomon_util_get_curr_timestamp()
 *
 * @brief                 Get the current timestamp
 *
 * @time_stamp:           Pointer to the time
 *
 * @return:               void
 */
void cam_common_util_get_curr_timestamp(struct timeval *time_stamp);
#endif /* _CAM_COMMON_UTIL_H_ */
