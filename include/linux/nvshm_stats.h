/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NVSHM_STATS_H
#define _NVSHM_STATS_H

#include <linux/notifier.h>

enum nvshm_stats_type {
	/** Start marker. */
	NVSHM_STATS_START,
	/** End marker. */
	NVSHM_STATS_END,
	/** Sub structure. */
	NVSHM_STATS_SUB,
	/** Unsigned 32bit integer. */
	NVSHM_STATS_UINT32,
	/** Signed 32bit integer. */
	NVSHM_STATS_SINT32,
	/** Unsigned 64bit integer. */
	NVSHM_STATS_UINT64,
};

enum nvshm_stats_notification {
	NVSHM_STATS_MODEM_UP,
	NVSHM_STATS_MODEM_DOWN,
};

struct nvshm_stats_desc;

struct nvshm_stats_iter {
	const struct nvshm_stats_desc *desc;
	char *data;
};

/**
 * Find entry in statistics given a top-structure type name.
 * @param top_name Name of top-structure to look for
 * @param it Iterator returned
 * @return Pointer to enabled field on success, error pointer on error
 */
const u32 *nvshm_stats_top(
	const char *top_name,
	struct nvshm_stats_iter *it);

/**
 * Enter sub-structure given a iterator on a NVSHM_STATS_SUB-type entry.
 * @param it Iterator on entry
 * @param index Array index to select if array of sub-structures
 * @param sub_it Iterator on sub-structure
 * @return 0 on success, negative error code on error
 */
int nvshm_stats_sub(
	const struct nvshm_stats_iter *it,
	int index,
	struct nvshm_stats_iter *sub_it);

/**
 * Increment iterator to point to next entry.
 * @param it Iterator to be incremented
 * @return 0 on success, negative error code on error
 */
int nvshm_stats_next(
	struct nvshm_stats_iter *it);

/**
 * Return name for entry pointed by iterator.
 * @param it Iterator on entry
 * @return Entry name
 */
const char *nvshm_stats_name(
	const struct nvshm_stats_iter *it);

/**
 * Return type for entry pointed by iterator.
 * @param it Iterator on entry
 * @return Entry type
 */
enum nvshm_stats_type nvshm_stats_type(
	const struct nvshm_stats_iter *it);

/**
 * Return number of elements in array.
 * If pointed data is not an array, the returned value is 1.
 * @param it Iterator on entry
 * @return Number of elements
 */
int nvshm_stats_elems(const struct nvshm_stats_iter *it);

/**
 * Get pointer to value for entry pointed by iterator.
 * @param it Iterator on entry
 * @param index Array index if applicable
 * @return Entry value pointer, or error pointer on failure
 */
u32 *nvshm_stats_valueptr_uint32(
	const struct nvshm_stats_iter *it,
	int index);

/**
 * Get pointer to value for entry pointed by iterator.
 * @param it Iterator on entry
 * @param index Array index if applicable
 * @return Entry value pointer, or error pointer on failure
 */
s32 *nvshm_stats_valueptr_sint32(
	const struct nvshm_stats_iter *it,
	int index);

/**
 * Get pointer to value for entry pointed by iterator.
 * @param it Iterator on entry
 * @param index Array index if applicable
 * @return Entry value pointer, or error pointer on failure
 */
u64 *nvshm_stats_valueptr_uint64(
	const struct nvshm_stats_iter *it,
	int index);

/**
 * Register for modem notifications.
 * @param nb notifier block to register
 */
void nvshm_stats_register(struct notifier_block *nb);

/**
 * Unregister from modem notifications.
 * @param nb notifier block to unregister
 */
void nvshm_stats_unregister(struct notifier_block *nb);

#endif /* _NVSHM_STATS_H */
