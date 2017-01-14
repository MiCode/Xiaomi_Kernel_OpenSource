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
#include "sde_hw_mdp_ctl.h"

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
