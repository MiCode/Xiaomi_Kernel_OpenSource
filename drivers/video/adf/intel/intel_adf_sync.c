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

#include "intel_adf_sync.h"

static struct sync_pt *intel_adf_sync_pt_dup(struct sync_pt *pt)
{
	struct intel_adf_sync_pt *new_pt, *old_pt;
	struct sync_timeline *tl;

	tl = pt->parent;
	old_pt = (struct intel_adf_sync_pt *)pt;

	new_pt = (struct intel_adf_sync_pt *)sync_pt_create(tl,
		sizeof(*new_pt));
	if (!new_pt)
		return NULL;

	atomic64_set(&new_pt->value, atomic64_read(&old_pt->value));
	return (struct sync_pt *)new_pt;
}

static int intel_adf_sync_pt_has_signaled(struct sync_pt *pt)
{
	struct intel_adf_sync_pt *i_pt = (struct intel_adf_sync_pt *)pt;
	struct intel_adf_sync_timeline *tl =
		(struct intel_adf_sync_timeline *)pt->parent;
	u64 tl_value, pt_value;

	tl_value = atomic64_read(&tl->value);
	pt_value = atomic64_read(&i_pt->value);

	return (tl_value >= pt_value) ? 1 : 0;
}

static int intel_adf_sync_pt_compare(struct sync_pt *a, struct sync_pt *b)
{
	struct intel_adf_sync_pt *a_pt = (struct intel_adf_sync_pt *)a;
	struct intel_adf_sync_pt *b_pt = (struct intel_adf_sync_pt *)b;
	u64 a_value, b_value, delta;
	int ret;

	a_value = atomic64_read(&a_pt->value);
	b_value = atomic64_read(&b_pt->value);
	delta = a_value - b_value;

	if (delta > 0)
		ret = 1;
	else if (delta < 0)
		ret = -1;
	else
		ret = 0;

	return ret;
}

static void intel_adf_sync_timeline_value_str(struct sync_timeline *timeline,
	char *str, int size)
{
	struct intel_adf_sync_timeline *tl =
		(struct intel_adf_sync_timeline *)timeline;
	snprintf(str, size, "%#lx", atomic64_read(&tl->value));
}

static void intel_adf_sync_pt_value_str(struct sync_pt *pt, char *str,
	int size)
{
	struct intel_adf_sync_pt *i_pt =
		(struct intel_adf_sync_pt *)pt;
	snprintf(str, size, "%#lx", atomic64_read(&i_pt->value));
}

static const struct sync_timeline_ops intel_adf_sync_timeline_ops = {
	.driver_name = "intel-adf-sync",
	.dup = intel_adf_sync_pt_dup,
	.has_signaled = intel_adf_sync_pt_has_signaled,
	.compare = intel_adf_sync_pt_compare,
	.timeline_value_str = intel_adf_sync_timeline_value_str,
	.pt_value_str = intel_adf_sync_pt_value_str,
};

struct intel_adf_sync_timeline *intel_adf_sync_timeline_create(
	const char *name)
{
	struct intel_adf_sync_timeline *tl;

	tl = (struct intel_adf_sync_timeline *)sync_timeline_create(
		&intel_adf_sync_timeline_ops, sizeof(*tl), name);
	if (!tl)
		return ERR_PTR(-ENOMEM);

	atomic64_set(&tl->value, 0);
	return tl;
}

void intel_adf_sync_timeline_destroy(struct intel_adf_sync_timeline *tl)
{
	if (tl)
		sync_timeline_destroy(&tl->base);
}

struct sync_fence *intel_adf_sync_fence_create(
	struct intel_adf_sync_timeline *tl, u64 value)
{
	struct sync_fence *fence;
	struct intel_adf_sync_pt *pt;

	if (!tl)
		return NULL;

	pt = (struct intel_adf_sync_pt *)sync_pt_create(&tl->base,
		sizeof(*pt));
	if (!pt)
		return NULL;

	atomic64_set(&pt->value, value);

	fence = sync_fence_create("intel_adf", &pt->base);
	if (!fence)
		goto out_err0;

	return fence;
out_err0:
	sync_pt_free(&pt->base);
	return NULL;
}

void intel_adf_sync_fence_put(struct intel_adf_sync_timeline *tl,
	struct sync_fence *fence)
{
	if (fence)
		sync_fence_put(fence);
}

void intel_adf_sync_timeline_signal(struct intel_adf_sync_timeline *tl,
	u64 value)
{
	if (tl) {
		atomic64_set(&tl->value, value);
		sync_timeline_signal(&tl->base);
	}
}
