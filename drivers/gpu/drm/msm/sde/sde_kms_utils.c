/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include "sde_kms.h"
#include "sde_hw_lm.h"
#include "sde_hw_ctl.h"

struct sde_hw_intr *sde_rm_acquire_intr(struct sde_kms *sde_kms)
{
	struct sde_hw_intr *hw_intr;

	if (!sde_kms) {
		DRM_ERROR("Invalid KMS Driver");
		return ERR_PTR(-EINVAL);
	}

	if (sde_kms->hw_res.intr) {
		DRM_ERROR("intr already in use ");
		return ERR_PTR(-ENODEV);
	}

	sde_enable(sde_kms);
	hw_intr = sde_hw_intr_init(sde_kms->mmio,
			sde_kms->catalog);
	sde_disable(sde_kms);

	if (!IS_ERR_OR_NULL(hw_intr))
		sde_kms->hw_res.intr = hw_intr;

	return hw_intr;
}

struct sde_hw_intr *sde_rm_get_intr(struct sde_kms *sde_kms)
{
	if (!sde_kms) {
		DRM_ERROR("Invalid KMS Driver");
		return ERR_PTR(-EINVAL);
	}

	return sde_kms->hw_res.intr;
}

struct sde_hw_ctl *sde_rm_acquire_ctl_path(struct sde_kms *sde_kms,
		enum sde_ctl idx)
{
	struct sde_hw_ctl *hw_ctl;

	if (!sde_kms) {
		DRM_ERROR("Invalid KMS driver");
		return ERR_PTR(-EINVAL);
	}

	if ((idx == SDE_NONE) || (idx > sde_kms->catalog->ctl_count)) {
		DRM_ERROR("Invalid Ctl Path Idx %d", idx);
		return ERR_PTR(-EINVAL);
	}

	if (sde_kms->hw_res.ctl[idx]) {
		DRM_ERROR("CTL path %d already in use ", idx);
		return ERR_PTR(-ENODEV);
	}

	sde_enable(sde_kms);
	hw_ctl = sde_hw_ctl_init(idx, sde_kms->mmio, sde_kms->catalog);
	sde_disable(sde_kms);

	if (!IS_ERR_OR_NULL(hw_ctl))
		sde_kms->hw_res.ctl[idx] = hw_ctl;

	return hw_ctl;
}

struct sde_hw_ctl *sde_rm_get_ctl_path(struct sde_kms *sde_kms,
	enum sde_ctl idx)
{
	if (!sde_kms) {
		DRM_ERROR("Invalid KMS Driver");
		return ERR_PTR(-EINVAL);
	}
	if ((idx == SDE_NONE) || (idx > sde_kms->catalog->ctl_count)) {
		DRM_ERROR("Invalid Ctl path Idx %d", idx);
		return ERR_PTR(-EINVAL);
	}

	return sde_kms->hw_res.ctl[idx];
}

void sde_rm_release_ctl_path(struct sde_kms *sde_kms, enum sde_ctl idx)
{
	if (!sde_kms) {
		DRM_ERROR("Invalid pointer\n");
		return;
	}
	if ((idx == SDE_NONE) || (idx > sde_kms->catalog->ctl_count)) {
		DRM_ERROR("Invalid Ctl path Idx %d", idx);
		return;
	}
}

struct sde_hw_mixer *sde_rm_acquire_mixer(struct sde_kms *sde_kms,
	enum sde_lm idx)
{
	struct sde_hw_mixer *mixer;

	if (!sde_kms) {
		DRM_ERROR("Invalid KMS Driver");
		return ERR_PTR(-EINVAL);
	}

	if ((idx == SDE_NONE) || (idx > sde_kms->catalog->mixer_count)) {
		DBG("Invalid mixer id %d", idx);
		return ERR_PTR(-EINVAL);
	}

	if (sde_kms->hw_res.mixer[idx]) {
		DRM_ERROR("mixer %d already in use ", idx);
		return ERR_PTR(-ENODEV);
	}

	sde_enable(sde_kms);
	mixer = sde_hw_lm_init(idx, sde_kms->mmio, sde_kms->catalog);
	sde_disable(sde_kms);

	if (!IS_ERR_OR_NULL(mixer))
		sde_kms->hw_res.mixer[idx] = mixer;

	return mixer;
}

struct sde_hw_mixer *sde_rm_get_mixer(struct sde_kms *sde_kms,
		enum sde_lm idx)
{
	if (!sde_kms) {
		DRM_ERROR("Invalid KMS Driver");
		return ERR_PTR(-EINVAL);
	}

	if ((idx == SDE_NONE) || (idx > sde_kms->catalog->mixer_count)) {
		DRM_ERROR("Invalid mixer id %d", idx);
		return ERR_PTR(-EINVAL);
	}

	return sde_kms->hw_res.mixer[idx];
}

const struct sde_hw_res_map *sde_rm_get_res_map(struct sde_kms *sde_kms,
		enum sde_intf idx)
{
	if (!sde_kms) {
		DRM_ERROR("Invalid KMS Driver");
		return ERR_PTR(-EINVAL);
	}
	if ((idx == SDE_NONE) || (idx > sde_kms->catalog->intf_count)) {
		DRM_ERROR("Invalid intf id %d", idx);
		return ERR_PTR(-EINVAL);
	}

	DBG(" Platform Resource map for INTF %d -> lm %d, pp %d ctl %d",
			sde_kms->hw_res.res_table[idx].intf,
			sde_kms->hw_res.res_table[idx].lm,
			sde_kms->hw_res.res_table[idx].pp,
			sde_kms->hw_res.res_table[idx].ctl);
	return &(sde_kms->hw_res.res_table[idx]);
}

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

