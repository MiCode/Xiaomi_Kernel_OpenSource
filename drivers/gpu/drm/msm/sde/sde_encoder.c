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

#include "msm_drv.h"
#include "sde_kms.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_intf.h"
#include "sde_hw_mdp_ctl.h"
#include "sde_mdp_formats.h"

#include "sde_encoder_phys.h"
#include "display_manager.h"

#define to_sde_encoder_virt(x) container_of(x, struct sde_encoder_virt, base)

#ifdef CONFIG_QCOM_BUS_SCALING
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#define MDP_BUS_VECTOR_ENTRY(ab_val, ib_val)		\
	{						\
		.src = MSM_BUS_MASTER_MDP_PORT0,	\
		.dst = MSM_BUS_SLAVE_EBI_CH0,		\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

static struct msm_bus_vectors mdp_bus_vectors[] = {
	MDP_BUS_VECTOR_ENTRY(0, 0),
	MDP_BUS_VECTOR_ENTRY(2000000000, 2000000000),
};

static struct msm_bus_paths mdp_bus_usecases[] = { {
						    .num_paths = 1,
						    .vectors =
						    &mdp_bus_vectors[0],
						    }, {
							.num_paths = 1,
							.vectors =
							&mdp_bus_vectors[1],
							}
};

static struct msm_bus_scale_pdata mdp_bus_scale_table = {
	.usecase = mdp_bus_usecases,
	.num_usecases = ARRAY_SIZE(mdp_bus_usecases),
	.name = "mdss_mdp",
};

static void bs_init(struct sde_encoder_virt *sde_enc)
{
	sde_enc->bus_scaling_client =
	    msm_bus_scale_register_client(&mdp_bus_scale_table);
	DBG("bus scale client: %08x", sde_enc->bus_scaling_client);
}

static void bs_fini(struct sde_encoder_virt *sde_enc)
{
	if (sde_enc->bus_scaling_client) {
		msm_bus_scale_unregister_client(sde_enc->bus_scaling_client);
		sde_enc->bus_scaling_client = 0;
	}
}

static void bs_set(struct sde_encoder_virt *sde_enc, int idx)
{
	if (sde_enc->bus_scaling_client) {
		DBG("set bus scaling: %d", idx);
		idx = 1;
		msm_bus_scale_client_update_request(sde_enc->bus_scaling_client,
						    idx);
	}
}
#else
static void bs_init(struct sde_encoder_virt *sde_enc)
{
}

static void bs_fini(struct sde_encoder_virt *sde_enc)
{
}

static void bs_set(struct sde_encoder_virt *sde_enc, int idx)
{
}
#endif

void sde_encoder_get_hw_resources(struct drm_encoder *drm_enc,
				  struct sde_encoder_hw_resources *hw_res)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;

	DBG("");

	if (!hw_res || !drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	/* Query resources used by phys encs, expected to be without overlap */
	memset(hw_res, 0, sizeof(*hw_res));
	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->phys_ops.get_hw_resources)
			phys->phys_ops.get_hw_resources(phys, hw_res);
	}
}

static void sde_encoder_destroy(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	for (i = 0; i < ARRAY_SIZE(sde_enc->phys_encs); i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->phys_ops.destroy) {
			phys->phys_ops.destroy(phys);
			--sde_enc->num_phys_encs;
			sde_enc->phys_encs[i] = NULL;
		}
	}

	if (sde_enc->num_phys_encs) {
		DRM_ERROR("Expected num_phys_encs to be 0 not %d\n",
				sde_enc->num_phys_encs);
	}

	drm_encoder_cleanup(drm_enc);
	bs_fini(sde_enc);
	kfree(sde_enc);
}

static bool sde_encoder_virt_mode_fixup(struct drm_encoder *drm_enc,
					const struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;
	bool ret = true;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return false;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->phys_ops.mode_fixup) {
			ret =
			    phys->phys_ops.mode_fixup(phys, mode,
						      adjusted_mode);
			if (!ret) {
				DBG("Mode unsupported by phys_enc %d", i);
				break;
			}

			if (sde_enc->num_phys_encs > 1) {
				DBG("ModeFix only checking 1 phys_enc");
				break;
			}
		}
	}

	return ret;
}

static void sde_encoder_virt_mode_set(struct drm_encoder *drm_enc,
				      struct drm_display_mode *mode,
				      struct drm_display_mode *adjusted_mode)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;
	bool splitmode = false;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	/*
	 * Panel is driven by two interfaces ,each interface drives half of
	 * the horizontal
	 */
	if (sde_enc->num_phys_encs == 2)
		splitmode = true;

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];
		if (phys) {
			phys->phys_ops.mode_set(phys,
					mode,
					adjusted_mode,
					splitmode);
			if (memcmp(mode, adjusted_mode, sizeof(*mode)) != 0)
				DRM_ERROR("adjusted modes not supported\n");
		}
	}
}

static void sde_encoder_virt_enable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;
	bool splitmode = false;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	bs_set(sde_enc, 1);

	if (sde_enc->num_phys_encs == 2)
		splitmode = true;


	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->phys_ops.enable)

			/* enable/disable dual interface top config */
			if (phys->phys_ops.enable_split_config)
				phys->phys_ops.enable_split_config(phys,
						splitmode);
			phys->phys_ops.enable(phys);
	}
}

static void sde_encoder_virt_disable(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	int i = 0;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	for (i = 0; i < sde_enc->num_phys_encs; i++) {
		struct sde_encoder_phys *phys = sde_enc->phys_encs[i];

		if (phys && phys->phys_ops.disable)
			phys->phys_ops.disable(phys);
	}

	bs_set(sde_enc, 0);
}

static const struct drm_encoder_helper_funcs sde_encoder_helper_funcs = {
	.mode_fixup = sde_encoder_virt_mode_fixup,
	.mode_set = sde_encoder_virt_mode_set,
	.disable = sde_encoder_virt_disable,
	.enable = sde_encoder_virt_enable,
};

static const struct drm_encoder_funcs sde_encoder_funcs = {
		.destroy = sde_encoder_destroy,
};

static enum sde_intf sde_encoder_get_intf(struct sde_mdss_cfg *catalog,
		enum sde_intf_type type, u32 controller_id)
{
	int i = 0;

	DBG("");

	for (i = 0; i < catalog->intf_count; i++) {
		if (catalog->intf[i].type == type
		    && catalog->intf[i].controller_id == controller_id) {
			return catalog->intf[i].id;
		}
	}

	return INTF_MAX;
}

static void sde_encoder_vblank_callback(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc = NULL;
	unsigned long lock_flags;

	DBG("");

	if (!drm_enc) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	spin_lock_irqsave(&sde_enc->spin_lock, lock_flags);
	if (sde_enc->kms_vblank_callback)
		sde_enc->kms_vblank_callback(sde_enc->kms_vblank_callback_data);
	spin_unlock_irqrestore(&sde_enc->spin_lock, lock_flags);
}

static int sde_encoder_virt_add_phys_vid_enc(struct sde_encoder_virt *sde_enc,
					     struct sde_kms *sde_kms,
					     enum sde_intf intf_idx,
					     enum sde_ctl ctl_idx)
{
	int ret = 0;

	DBG("");

	if (sde_enc->num_phys_encs >= ARRAY_SIZE(sde_enc->phys_encs)) {
		DRM_ERROR("Too many video encoders %d, unable to add\n",
			  sde_enc->num_phys_encs);
		ret = -EINVAL;
	} else {
		struct sde_encoder_virt_ops parent_ops = {
			sde_encoder_vblank_callback
		};
		struct sde_encoder_phys *enc =
		    sde_encoder_phys_vid_init(sde_kms, intf_idx, ctl_idx,
					      &sde_enc->base,
					      parent_ops);
		if (IS_ERR(enc))
			ret = PTR_ERR(enc);

		if (!ret) {
			sde_enc->phys_encs[sde_enc->num_phys_encs] = enc;
			++sde_enc->num_phys_encs;
		}
	}

	return ret;
}

static int sde_encoder_setup_display(struct sde_encoder_virt *sde_enc,
				 struct sde_kms *sde_kms,
				 struct display_info *disp_info,
				 int *drm_enc_mode)
{
	int ret = 0;
	int i = 0;
	enum sde_intf_type intf_type = INTF_NONE;

	DBG("");

	if (disp_info->intf == DISPLAY_INTF_DSI) {
		*drm_enc_mode = DRM_MODE_ENCODER_DSI;
		intf_type = INTF_DSI;
	} else if (disp_info->intf == DISPLAY_INTF_HDMI) {
		*drm_enc_mode = DRM_MODE_ENCODER_TMDS;
		intf_type = INTF_HDMI;
	} else {
		DRM_ERROR("Unsupported display interface type");
		return -EINVAL;
	}

	WARN_ON(disp_info->num_of_h_tiles < 1);

	DBG("dsi_info->num_of_h_tiles %d", disp_info->num_of_h_tiles);

	for (i = 0; i < disp_info->num_of_h_tiles && !ret; i++) {
		/*
		 * Left-most tile is at index 0, content is controller id
		 * h_tile_instance_ids[2] = {0, 1}; DSI0 = left, DSI1 = right
		 * h_tile_instance_ids[2] = {1, 0}; DSI1 = left, DSI0 = right
		 */
		const struct sde_hw_res_map *hw_res_map = NULL;
		enum sde_intf intf_idx = INTF_MAX;
		enum sde_ctl ctl_idx = CTL_MAX;
		u32 controller_id = disp_info->h_tile_instance[i];

		DBG("h_tile_instance %d = %d", i, controller_id);

		intf_idx = sde_encoder_get_intf(sde_kms->catalog,
				intf_type, controller_id);
		if (intf_idx == INTF_MAX) {
			DBG("Error: could not get the interface id");
			ret = -EINVAL;
		}

		hw_res_map = sde_rm_get_res_map(sde_kms, intf_idx);
		if (IS_ERR_OR_NULL(hw_res_map))
			ret = -EINVAL;
		else
			ctl_idx = hw_res_map->ctl;

		/* Create both VID and CMD Phys Encoders here */
		if (!ret)
			ret = sde_encoder_virt_add_phys_vid_enc(
					sde_enc, sde_kms, intf_idx, ctl_idx);
	}


	return ret;
}

static struct drm_encoder *sde_encoder_virt_init(
		struct drm_device *dev, struct display_info *disp_info)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	struct drm_encoder *drm_enc = NULL;
	struct sde_encoder_virt *sde_enc = NULL;
	int drm_enc_mode = DRM_MODE_ENCODER_NONE;
	int ret = 0;

	DBG("");

	sde_enc = kzalloc(sizeof(*sde_enc), GFP_KERNEL);
	if (!sde_enc) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = sde_encoder_setup_display(sde_enc, sde_kms, disp_info,
			&drm_enc_mode);
	if (ret)
		goto fail;

	spin_lock_init(&sde_enc->spin_lock);
	drm_enc = &sde_enc->base;
	drm_encoder_init(dev, drm_enc, &sde_encoder_funcs, drm_enc_mode);
	drm_encoder_helper_add(drm_enc, &sde_encoder_helper_funcs);
	bs_init(sde_enc);

	DBG("Created encoder");

	return drm_enc;

fail:
	DRM_ERROR("Failed to create encoder\n");
	if (drm_enc)
		sde_encoder_destroy(drm_enc);

	return ERR_PTR(ret);
}

void sde_encoder_register_vblank_callback(struct drm_encoder *drm_enc,
		void (*cb)(void *), void *data)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	unsigned long lock_flags;

	DBG("");

	spin_lock_irqsave(&sde_enc->spin_lock, lock_flags);
	sde_enc->kms_vblank_callback = cb;
	sde_enc->kms_vblank_callback_data = data;
	spin_unlock_irqrestore(&sde_enc->spin_lock, lock_flags);
}

void  sde_encoder_get_vsync_info(struct drm_encoder *drm_enc,
		struct vsync_info *vsync)
{
	struct sde_encoder_virt *sde_enc = to_sde_encoder_virt(drm_enc);
	struct sde_encoder_phys *phys;

	DBG("");

	if (!vsync) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	/* we get the vsync info from the intf at index 0: master index */
	phys = sde_enc->phys_encs[0];
	if (phys)
		phys->phys_ops.get_vsync_info(phys, vsync);
}

/* encoders init,
 * initialize encoder based on displays
 */
void sde_encoders_init(struct drm_device *dev)
{
	struct msm_drm_private *priv = NULL;
	struct display_manager *disp_man = NULL;
	u32 i = 0;
	u32 num_displays = 0;

	DBG("");

	if (!dev || !dev->dev_private) {
		DRM_ERROR("Invalid pointer");
		return;
	}

	priv = dev->dev_private;
	priv->num_encoders = 0;
	if (!priv->kms || !priv->dm) {
		DRM_ERROR("Invalid pointer");
		return;
	}
	disp_man = priv->dm;

	num_displays = display_manager_get_count(disp_man);
	DBG("num_displays %d", num_displays);

	if (num_displays > ARRAY_SIZE(priv->encoders)) {
		num_displays = ARRAY_SIZE(priv->encoders);
		DRM_ERROR("Too many displays found, capping to %d",
				num_displays);
	}

	for (i = 0; i < num_displays; i++) {
		struct display_info info = { 0 };
		struct drm_encoder *enc = NULL;
		u32 ret = 0;

		ret = display_manager_get_info_by_index(disp_man, i, &info);
		if (ret) {
			DRM_ERROR("Failed to get display info, %d", ret);
			return;
		}

		enc = sde_encoder_virt_init(dev, &info);
		if (IS_ERR_OR_NULL(enc)) {
			DRM_ERROR("Encoder initialization failed");
			return;
		}

		ret = display_manager_drm_init_by_index(disp_man, i, enc);
		if (ret) {
			DRM_ERROR("Display drm_init failed, %d", ret);
			return;
		}

		priv->encoders[priv->num_encoders++] = enc;
	}
}
