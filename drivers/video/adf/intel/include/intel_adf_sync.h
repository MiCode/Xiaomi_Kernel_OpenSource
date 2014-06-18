/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef INTEL_ADF_SYNC_H_
#define INTEL_ADF_SYNC_H_

#include <android/sync.h>

struct intel_adf_sync_timeline {
	struct sync_timeline base;
	atomic64_t value;
};

struct intel_adf_sync_pt {
	struct sync_pt base;
	atomic64_t value;
};

extern struct intel_adf_sync_timeline *intel_adf_sync_timeline_create(
	const char *name);
extern void intel_adf_sync_timeline_destroy(
	struct intel_adf_sync_timeline *tl);
extern struct sync_fence *intel_adf_sync_fence_create(
	struct intel_adf_sync_timeline *tl, u64 value);
extern void intel_adf_sync_fence_put(struct intel_adf_sync_timeline *tl,
	struct sync_fence *fence);
extern void intel_adf_sync_timeline_signal(
	struct intel_adf_sync_timeline *tl, u64 value);

#endif /* INTEL_ADF_SYNC_H_ */
