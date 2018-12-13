/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
		int32_t value)
{
	uint32_t len;

	if (info && key) {
		len = snprintf(info->data + info->len,
				SDE_KMS_INFO_MAX_SIZE - info->len,
				"%s=%d\n",
				key,
				value);

		/* check if snprintf truncated the string */
		if ((info->len + len) < SDE_KMS_INFO_MAX_SIZE)
			info->len += len;
	}
}

void sde_kms_info_update_keystr(char *info_str,
				const char *key,
				int32_t value)
{
	char *str, *temp, *append_str;
	uint32_t dst_len = 0, prefix_len = 0;
	char c;
	int32_t size = 0;

	if (info_str && key) {
		str = strnstr(info_str, key, strlen(info_str));
		if (str) {
			temp = str + strlen(key);
			c = *temp;
			while (c != '\n') {
				dst_len++;
				c = *(++temp);
			}
			/*
			 * If input key string to update is exactly the last
			 * string in source string, no need to allocate one
			 * memory to store the string after string key. Just
			 * replace the value of the last string.
			 *
			 * If it is not, allocate one new memory to save
			 * the string after string key+"\n". This new allocated
			 * string will be appended to the whole source string
			 * after key value is updated.
			 */
			size = strlen(str) - strlen(key) - dst_len - 1;
			if (size > 0) {
				append_str = kzalloc(size + 1, GFP_KERNEL);
				if (!append_str) {
					SDE_ERROR("failed to alloc memory\n");
					return;
				}
				memcpy(append_str,
					str + strlen(key) + dst_len + 1, size);
			}

			prefix_len = strlen(info_str) - strlen(str);
			/* Update string with new value for the string key. */
			snprintf(info_str + prefix_len,
				SDE_KMS_INFO_MAX_SIZE - prefix_len,
				"%s%d\n", key, value);

			/* Append the string save aboved. */
			if (size > 0 && append_str) {
				size = prefix_len + strlen(key) + dst_len + 1;
				snprintf(info_str + size,
					SDE_KMS_INFO_MAX_SIZE - size,
					"%s", append_str);
				kfree(append_str);
			}
		}
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

	if (r < l || b < t) {
		memset(result, 0, sizeof(*result));
	} else {
		result->x = l;
		result->y = t;
		result->w = r - l;
		result->h = b - t;
	}
}
