/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"sde-kms_utils:[%s] " fmt, __func__

#include "sde_kms.h"

void sde_kms_info_reset(struct sde_kms_info *info)
{
	if (info) {
		info->len = 0;
		info->staged_len = 0;
	}
}

void sde_kms_info_add_keyint(struct sde_kms_info *info,
		const char *key,
		int64_t value)
{
	uint32_t len;

	if (info && key) {
		len = snprintf(info->data + info->len,
				SDE_KMS_INFO_MAX_SIZE - info->len,
				"%s=%lld\n",
				key,
				value);

		/* check if snprintf truncated the string */
		if ((info->len + len) < SDE_KMS_INFO_MAX_SIZE)
			info->len += len;
	}
}

void sde_kms_info_add_keystr(struct sde_kms_info *info,
		const char *key,
		const char *value)
{
	uint32_t len;

	if (info && key && value) {
		len = snprintf(info->data + info->len,
				SDE_KMS_INFO_MAX_SIZE - info->len,
				"%s=%s\n",
				key,
				value);

		/* check if snprintf truncated the string */
		if ((info->len + len) < SDE_KMS_INFO_MAX_SIZE)
			info->len += len;
	}
}

void sde_kms_info_start(struct sde_kms_info *info,
		const char *key)
{
	uint32_t len;

	if (info && key) {
		len = snprintf(info->data + info->len,
				SDE_KMS_INFO_MAX_SIZE - info->len,
				"%s=",
				key);

		info->start = true;

		/* check if snprintf truncated the string */
		if ((info->len + len) < SDE_KMS_INFO_MAX_SIZE)
			info->staged_len = info->len + len;
	}
}

void sde_kms_info_append(struct sde_kms_info *info,
		const char *str)
{
	uint32_t len;

	if (info) {
		len = snprintf(info->data + info->staged_len,
				SDE_KMS_INFO_MAX_SIZE - info->staged_len,
				"%s",
				str);

		/* check if snprintf truncated the string */
		if ((info->staged_len + len) < SDE_KMS_INFO_MAX_SIZE) {
			info->staged_len += len;
			info->start = false;
		}
	}
}

void sde_kms_info_append_format(struct sde_kms_info *info,
		uint32_t pixel_format,
		uint64_t modifier)
{
	uint32_t len;

	if (!info)
		return;

	if (modifier) {
		len = snprintf(info->data + info->staged_len,
				SDE_KMS_INFO_MAX_SIZE - info->staged_len,
				info->start ?
				"%c%c%c%c/%llX/%llX" : " %c%c%c%c/%llX/%llX",
				(pixel_format >> 0) & 0xFF,
				(pixel_format >> 8) & 0xFF,
				(pixel_format >> 16) & 0xFF,
				(pixel_format >> 24) & 0xFF,
				(modifier >> 56) & 0xFF,
				modifier & ((1ULL << 56) - 1));
	} else {
		len = snprintf(info->data + info->staged_len,
				SDE_KMS_INFO_MAX_SIZE - info->staged_len,
				info->start ?
				"%c%c%c%c" : " %c%c%c%c",
				(pixel_format >> 0) & 0xFF,
				(pixel_format >> 8) & 0xFF,
				(pixel_format >> 16) & 0xFF,
				(pixel_format >> 24) & 0xFF);
	}

	/* check if snprintf truncated the string */
	if ((info->staged_len + len) < SDE_KMS_INFO_MAX_SIZE) {
		info->staged_len += len;
		info->start = false;
	}
}

void sde_kms_info_stop(struct sde_kms_info *info)
{
	uint32_t len;

	if (info) {
		/* insert final delimiter */
		len = snprintf(info->data + info->staged_len,
				SDE_KMS_INFO_MAX_SIZE - info->staged_len,
				"\n");

		/* check if snprintf truncated the string */
		if ((info->staged_len + len) < SDE_KMS_INFO_MAX_SIZE)
			info->len = info->staged_len + len;
	}
}

void sde_kms_rect_intersect(const struct sde_rect *r1,
		const struct sde_rect *r2,
		struct sde_rect *result)
{
	int l, t, r, b;

	if (!r1 || !r2 || !result)
		return;

	l = max(r1->x, r2->x);
	t = max(r1->y, r2->y);
	r = min((r1->x + r1->w), (r2->x + r2->w));
	b = min((r1->y + r1->h), (r2->y + r2->h));

	if (r <= l || b <= t) {
		memset(result, 0, sizeof(*result));
	} else {
		result->x = l;
		result->y = t;
		result->w = r - l;
		result->h = b - t;
	}
}

void sde_kms_rect_merge_rectangles(const struct msm_roi_list *rois,
		struct sde_rect *result)
{
	struct drm_clip_rect clip;
	const struct drm_clip_rect *roi_rect;
	int i;

	if (!rois || !result)
		return;

	memset(result, 0, sizeof(*result));

	/* init to invalid range maxes */
	clip.x1 = ~0;
	clip.y1 = ~0;
	clip.x2 = 0;
	clip.y2 = 0;

	/* aggregate all clipping rectangles together for overall roi */
	for (i = 0; i < rois->num_rects; i++) {
		roi_rect = &rois->roi[i];

		clip.x1 = min(clip.x1, roi_rect->x1);
		clip.y1 = min(clip.y1, roi_rect->y1);
		clip.x2 = max(clip.x2, roi_rect->x2);
		clip.y2 = max(clip.y2, roi_rect->y2);

		SDE_DEBUG("roi%d (%d,%d),(%d,%d) -> crtc (%d,%d),(%d,%d)\n", i,
				roi_rect->x1, roi_rect->y1,
				roi_rect->x2, roi_rect->y2,
				clip.x1, clip.y1,
				clip.x2, clip.y2);
	}

	if (clip.x2  && clip.y2) {
		result->x = clip.x1;
		result->y = clip.y1;
		result->w = clip.x2 - clip.x1;
		result->h = clip.y2 - clip.y1;
	}
}

