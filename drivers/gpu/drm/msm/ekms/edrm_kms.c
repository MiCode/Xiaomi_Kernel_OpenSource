/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <drm/drm_crtc.h>
#include <linux/debugfs.h>
#include <soc/qcom/boot_stats.h>
#include "msm_kms.h"
#include "edrm_kms.h"
#include "edrm_crtc.h"
#include "edrm_encoder.h"
#include "edrm_plane.h"
#include "edrm_connector.h"
#include "sde_kms.h"
#include "sde_formats.h"
#include "edrm_splash.h"
#include "sde_hdmi.h"
#include "dsi_display.h"
#include "sde_crtc.h"

#define MMSS_MDP_CTL_TOP_OFFSET 0x14

static bool first_commit = true;

static void edrm_kms_prepare_commit(struct msm_kms *kms,
		struct drm_atomic_state *state)
{
	struct msm_edrm_kms *edrm_kms = to_edrm_kms(kms);
	struct drm_device *dev = edrm_kms->master_dev;
	struct msm_drm_private *master_priv = edrm_kms->master_dev->dev_private;
	struct sde_kms *master_kms;
	int i, nplanes;
	struct drm_plane *plane;
	bool valid_commit = false;

	master_kms = to_sde_kms(master_priv->kms);
	nplanes = dev->mode_config.num_total_plane;
	for (i = 0; i < nplanes; i++) {
		plane = state->planes[i];
		if (plane && plane->fb) {
			valid_commit = true;
			break;
		}
	}

	if (valid_commit && first_commit) {
		first_commit = false;
		place_marker("eDRM display first valid commit");
	}

	sde_power_resource_enable(&master_priv->phandle,
			master_kms->core_client, true);

	/* Notify bootloader splash to stop */
	if (valid_commit && edrm_kms->lk_running_flag) {


		/* next eDRM close will trigger display resources handoff */
		edrm_kms->handoff_flag = true;
	}
}

static void edrm_kms_commit(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		if (crtc->state->active)
			edrm_crtc_commit_kickoff(crtc);
	}
}

static void edrm_kms_complete_commit(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct msm_edrm_kms *edrm_kms = to_edrm_kms(kms);
	struct msm_drm_private *master_priv = edrm_kms->master_dev->dev_private;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct sde_kms *master_kms;
	int i;

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i)
		edrm_crtc_complete_commit(crtc, old_crtc_state);

	master_kms = to_sde_kms(master_priv->kms);
	sde_power_resource_enable(&master_priv->phandle,
		master_kms->core_client, false);
}

static void edrm_kms_wait_for_commit_done(struct msm_kms *kms,
		struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev;
	int ret;

	dev = crtc->dev;
	if (!dev)
		return;

	if (!crtc->state->enable) {
		pr_err("[crtc:%d] not enable\n", crtc->base.id);
		return;
	}

	if (!crtc->state->active) {
		pr_err("[crtc:%d] not active\n", crtc->base.id);
		return;
	}

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;
		ret = edrm_encoder_wait_for_commit_done(encoder);
		if (ret && ret != -EWOULDBLOCK) {
			pr_err("wait for commit done returned %d\n", ret);
			break;
		}
	}
}

static void edrm_kms_prepare_fence(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	if (!kms || !old_state || !old_state->dev || !old_state->acquire_ctx) {
		pr_err("invalid argument(s)\n");
		return;
	}

	/* old_state contains updated crtc pointers */
	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i)
		edrm_crtc_prepare_commit(crtc, old_crtc_state);
}

static void _edrm_kms_drm_obj_destroy(struct msm_edrm_kms *edrm_kms)
{
	struct msm_drm_private *priv;
	int i;

	if (!edrm_kms) {
		pr_err("invalid sde_kms\n");
		return;
	} else if (!edrm_kms->dev) {
		pr_err("invalid dev\n");
		return;
	} else if (!edrm_kms->dev->dev_private) {
		pr_err("invalid dev_private\n");
		return;
	}
	priv = edrm_kms->dev->dev_private;

	for (i = 0; i < priv->num_crtcs; i++)
		priv->crtcs[i]->funcs->destroy(priv->crtcs[i]);
	priv->num_crtcs = 0;

	for (i = 0; i < priv->num_planes; i++)
		priv->planes[i]->funcs->destroy(priv->planes[i]);
	priv->num_planes = 0;

	for (i = 0; i < priv->num_connectors; i++)
		priv->connectors[i]->funcs->destroy(priv->connectors[i]);
	priv->num_connectors = 0;

	for (i = 0; i < priv->num_encoders; i++)
		priv->encoders[i]->funcs->destroy(priv->encoders[i]);
	priv->num_encoders = 0;
}

static void convert_dsi_to_drm_mode(const struct dsi_display_mode *dsi_mode,
				struct drm_display_mode *drm_mode)
{
	memset(drm_mode, 0, sizeof(*drm_mode));

	drm_mode->hdisplay = dsi_mode->timing.h_active;
	drm_mode->hsync_start = drm_mode->hdisplay +
				dsi_mode->timing.h_front_porch;
	drm_mode->hsync_end = drm_mode->hsync_start +
				dsi_mode->timing.h_sync_width;
	drm_mode->htotal = drm_mode->hsync_end + dsi_mode->timing.h_back_porch;
	drm_mode->hskew = dsi_mode->timing.h_skew;

	drm_mode->vdisplay = dsi_mode->timing.v_active;
	drm_mode->vsync_start = drm_mode->vdisplay +
				dsi_mode->timing.v_front_porch;
	drm_mode->vsync_end = drm_mode->vsync_start +
			      dsi_mode->timing.v_sync_width;
	drm_mode->vtotal = drm_mode->vsync_end + dsi_mode->timing.v_back_porch;

	drm_mode->vrefresh = dsi_mode->timing.refresh_rate;
	drm_mode->clock = dsi_mode->pixel_clk_khz;

	if (dsi_mode->flags & DSI_MODE_FLAG_SEAMLESS)
		drm_mode->flags |= DRM_MODE_FLAG_SEAMLESS;
	if (dsi_mode->flags & DSI_MODE_FLAG_DFPS)
		drm_mode->private_flags |= MSM_MODE_FLAG_SEAMLESS_DYNAMIC_FPS;
	if (dsi_mode->flags & DSI_MODE_FLAG_VBLANK_PRE_MODESET)
		drm_mode->private_flags |= MSM_MODE_FLAG_VBLANK_PRE_MODESET;
	drm_mode->flags |= (dsi_mode->timing.h_sync_polarity) ?
				DRM_MODE_FLAG_NHSYNC : DRM_MODE_FLAG_PHSYNC;
	drm_mode->flags |= (dsi_mode->timing.v_sync_polarity) ?
				DRM_MODE_FLAG_NVSYNC : DRM_MODE_FLAG_PVSYNC;

	drm_mode_set_name(drm_mode);
}

static int setup_edrm_displays(struct sde_kms *master_kms,
			struct msm_edrm_display *display,
			const char *label, const char *type)
{
	int i, ret;
	struct dsi_display *dsi_disp;
	struct sde_hdmi *hdmi_display;
	struct sde_mdss_cfg *cfg;
	u32 reg_value;

	cfg = master_kms->catalog;
	ret = -EINVAL;
	/* check main DRM for the matching display */
	if (!strcmp(type, "dsi")) {
		int mode_cnt;
		struct dsi_display_mode *dsi_mode;
		/* check main DRM's DSI display list */
		for (i = 0; i < master_kms->dsi_display_count; i++) {
			dsi_disp = (struct dsi_display *)
				master_kms->dsi_displays[i];
			if (!strcmp(dsi_disp->name, label)) {
				dsi_display_get_modes(dsi_disp, NULL,
					&mode_cnt);
				dsi_mode = kcalloc(mode_cnt, sizeof(*dsi_mode),
					GFP_KERNEL);
				if (!dsi_mode)
					return -ENOMEM;
				dsi_display_get_modes(dsi_disp, dsi_mode,
					&mode_cnt);

				/* convert to DRM mode */
				convert_dsi_to_drm_mode(&dsi_mode[0],
					&display->mode);
				display->encoder_type = DRM_MODE_ENCODER_DSI;
				display->connector_type =
						DRM_MODE_CONNECTOR_DSI;
				ret = 0;
				break;
			}
		}
		if (ret) {
			pr_err("Cannot find %s in main DRM\n", label);
			return ret;
		}
		ret = -EINVAL;
		for (i = 0; i < cfg->ctl_count; i++) {
			reg_value = readl_relaxed(master_kms->mmio +
				cfg->ctl[i].base + MMSS_MDP_CTL_TOP_OFFSET);
			reg_value &= 0x000000F0;

			/* Check the interface from TOP register */
			if ((((reg_value >> 4) == 0x2) &&
				(dsi_disp->ctrl[0].ctrl->index == 0)) ||
				(((reg_value >> 4) == 0x3) &&
				(dsi_disp->ctrl[0].ctrl->index == 1))) {
				display->ctl_id = i + 1;
				display->ctl_off = cfg->ctl[i].base;
				display->lm_off = cfg->mixer[i].base;
				ret = 0;
				break;
			}
		}
		if (ret) {
			pr_err("LK does not enable %s\n", label);
			kfree(dsi_mode);
			return -EINVAL;
		}
	} else if (!strcmp(type, "hdmi")) {
		/* for HDMI interface, check main DRM's HDMI display list */
		for (i = 0; i < master_kms->hdmi_display_count; i++) {
			hdmi_display = (struct sde_hdmi *)
				master_kms->hdmi_displays[i];

			if (!strcmp(hdmi_display->name, label)) {
				drm_mode_copy(&display->mode,
					(struct drm_display_mode *)
					hdmi_display->mode_list.next);
				display->encoder_type = DRM_MODE_ENCODER_TMDS;
				display->connector_type =
					DRM_MODE_CONNECTOR_HDMIA;
				ret = 0;
				break;
			}
		}
		if (ret) {
			pr_err("Cannot find %s in main DRM\n", label);
			return ret;
		}
		ret = -EINVAL;
		for (i = 0; i < cfg->ctl_count; i++) {
			reg_value = readl_relaxed(master_kms->mmio +
				cfg->ctl[i].base + MMSS_MDP_CTL_TOP_OFFSET);
			reg_value &= 0x000000F0;

			/* Check the interface from TOP register */
			if ((reg_value >> 4) == 0x4) {
				display->ctl_id = i + 1;
				display->ctl_off = cfg->ctl[i].base;
				display->lm_off = cfg->mixer[i].base;
				ret = 0;
				break;
			}
		}
		if (ret) {
			pr_err("No LK does not enable %s\n", label);
			return -EINVAL;
		}
	}
	return ret;
}

static int _sspp_search(const char *p_name, struct sde_mdss_cfg *cfg,
			u32 *sspp_offset, u32 *sspp_cfg_id, u32 *sspp_type)
{
	int i, ret;

	ret = -1;
	for (i = 0; i < cfg->sspp_count; i++)
		if (!strcmp(cfg->sspp[i].name, p_name)) {
			*sspp_offset = cfg->sspp[i].base;
			*sspp_cfg_id = cfg->sspp[i].id;
			*sspp_type = cfg->sspp[i].type;
			ret = 0;
			break;
		}
	return ret;
}

static int _edrm_kms_parse_dt(struct msm_edrm_kms *edrm_kms)
{
	struct sde_kms *master_kms;
	struct msm_drm_private *master_priv;
	struct msm_drm_private *priv;
	struct sde_mdss_cfg *cfg;
	struct device_node *parent, *node;
	int i, ret, disp_cnt, plane_cnt;
	const char *clabel;
	const char *ctype;
	struct device_node *plane_node;
	struct drm_plane *plane;
	struct drm_crtc *crtc;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct edrm_plane *edrm_plane;
	const char *p_name;
	u32 lm_stage, sspp_offset, sspp_cfg_id, sspp_type;

	master_priv = edrm_kms->master_dev->dev_private;
	master_kms = to_sde_kms(master_priv->kms);
	priv = edrm_kms->dev->dev_private;
	cfg = master_kms->catalog;
	ret = 0;
	parent = of_get_child_by_name(edrm_kms->dev->dev->of_node,
		"qcom,edrm-assigned-display");
	if (!parent) {
		pr_err("cannot find qcom,edrm-assigned-display\n");
		return 0;
	}

	/* parse the dtsi and retrieve information from main DRM */
	disp_cnt = 0;
	for_each_child_of_node(parent, node) {
		of_property_read_string(node, "qcom,intf-type", &ctype);
		of_property_read_string(node, "qcom,label", &clabel);

		plane_cnt = 0;
		do {
			plane_node = of_parse_phandle(node,
				"qcom,assigned_plane", plane_cnt);
			/* Initialize plane */
			if (!plane_node)
				break;

			of_property_read_string(plane_node, "qcom,plane-name",
					&p_name);
			of_property_read_u32(plane_node, "lm-stage",
					&lm_stage);
			if (_sspp_search(p_name, cfg, &sspp_offset,
				&sspp_cfg_id, &sspp_type)) {
				pr_err("Cannot find %s in main DRM\n",
					p_name);
				continue;
			}

			plane = edrm_plane_init(edrm_kms->dev,
					edrm_kms->plane_id[disp_cnt],
					sspp_type);
			if (IS_ERR(plane)) {
				pr_err("edrm_plane_init failed\n");
				ret = PTR_ERR(plane);
				of_node_put(plane_node);
				goto fail;
			}
			priv->planes[priv->num_planes] = plane;
			edrm_plane = to_edrm_plane(plane);
			edrm_plane->display_id = disp_cnt;
			edrm_plane->lm_stage = lm_stage;
			edrm_plane->sspp_offset = sspp_offset;
			edrm_plane->sspp_cfg_id = sspp_cfg_id;
			edrm_plane->sspp_type = sspp_type;
			plane->possible_crtcs = (1 << disp_cnt);
			priv->num_planes++;
			plane_cnt++;
			of_node_put(plane_node);
		} while (plane_node);

		edrm_kms->display[disp_cnt].plane_cnt = plane_cnt;
		ret = setup_edrm_displays(master_kms,
			&edrm_kms->display[disp_cnt], clabel, ctype);
		if (ret)
			goto fail;

		/* Initialize crtc */
		crtc = edrm_crtc_init(edrm_kms->dev,
			&edrm_kms->display[disp_cnt], priv->planes[disp_cnt]);
		if (IS_ERR(crtc)) {
			ret = PTR_ERR(crtc);
			goto fail;
		}
		priv->crtcs[priv->num_crtcs++] = crtc;

		/* Initialize encoder */
		encoder = edrm_encoder_init(edrm_kms->dev,
			&edrm_kms->display[disp_cnt]);
		if (IS_ERR(encoder)) {
			ret = PTR_ERR(encoder);
			goto fail;
		}
		encoder->possible_crtcs = (1 << disp_cnt);
		priv->encoders[priv->num_encoders++] = encoder;

		/* Initialize connector */
		connector = edrm_connector_init(edrm_kms->dev,
				priv->encoders[disp_cnt],
				&edrm_kms->display[disp_cnt]);
		if (IS_ERR(encoder)) {
			ret = PTR_ERR(connector);
			goto fail;
		}
		priv->connectors[priv->num_connectors++] = connector;

		disp_cnt++;
	}
	of_node_put(parent);

	edrm_kms->display_count = disp_cnt;
	edrm_kms->plane_count = priv->num_planes;
	return ret;
fail:
	for (i = 0; i < priv->num_planes; i++)
		edrm_plane_destroy(priv->planes[i]);
	priv->num_planes = 0;

	for (i = 0; i < disp_cnt; i++) {
		if (priv->crtcs[i]) {
			edrm_crtc_destroy(priv->crtcs[i]);
			priv->num_crtcs--;
		}
		if (priv->encoders[i]) {
			edrm_encoder_destroy(priv->encoders[i]);
			priv->num_encoders--;
		}
		if (priv->connectors[i]) {
			edrm_connector_destroy(priv->connectors[i]);
			priv->num_connectors--;
		}
	}
	disp_cnt = 0;
	edrm_kms->display_count = 0;
	edrm_kms->plane_count = 0;
	of_node_put(parent);
	return ret;
}

static int _edrm_kms_drm_obj_init(struct msm_edrm_kms *edrm_kms)
{
	struct drm_device *dev;
	struct msm_drm_private *priv;
	int ret;

	if (!edrm_kms || !edrm_kms->dev || !edrm_kms->dev->dev) {
		pr_err("invalid edrm_kms\n");
		return -EINVAL;
	}

	dev = edrm_kms->dev;
	priv = dev->dev_private;

	ret = _edrm_kms_parse_dt(edrm_kms);
	if (ret)
		goto fail;

	return 0;
fail:
	_edrm_kms_drm_obj_destroy(edrm_kms);
	return ret;
}

static int edrm_kms_postinit(struct msm_kms *kms)
{
	struct drm_device *dev;
	struct drm_crtc *crtc;
	struct msm_edrm_kms *edrm_kms;

	edrm_kms = to_edrm_kms(kms);
	dev = edrm_kms->dev;

	drm_for_each_crtc(crtc, dev)
		edrm_crtc_postinit(crtc);

	place_marker("eDRM driver init completed");
	return 0;
}

static void edrm_kms_destroy(struct msm_kms *kms)
{
	struct msm_edrm_kms *edrm_kms;
	struct drm_device *dev;

	if (!kms) {
		pr_err("edrm_kms_destroy invalid kms\n");
		return;
	}

	edrm_kms = to_edrm_kms(kms);
	dev = edrm_kms->dev;
	if (!dev) {
		pr_err("invalid device\n");
		return;
	}

	kfree(edrm_kms);
}

static void edrm_kms_lastclose(struct msm_kms *kms)
{
	/* handoff early drm resource */
	struct msm_edrm_kms *edrm_kms = to_edrm_kms(kms);

	/* notify main DRM that eDRM is relased.  main DRM can
	 * reclaim all eDRM resource. Main DRM will clear eDRM
	 * plane stage in next commit
	 */
	if (edrm_kms->handoff_flag) {
		pr_info("handoff eDRM resource to main DRM\n");
		edrm_display_release(kms);
	}
}

static int edrm_kms_hw_init(struct msm_kms *kms)
{
	struct msm_edrm_kms *edrm_kms;
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct msm_drm_private *master_priv;
	int rc = -EINVAL;
	u32 lk_status;

	if (!kms) {
		pr_err("edrm_kms_hw_init invalid kms\n");
		goto error;
	}

	edrm_kms = to_edrm_kms(kms);
	dev = edrm_kms->dev;
	if (!dev || !dev->platformdev) {
		pr_err("invalid device\n");
		goto error;
	}

	priv = dev->dev_private;
	if (!priv) {
		pr_err("invalid private data\n");
		goto error;
	}

	master_priv = edrm_kms->master_dev->dev_private;
	sde_kms = to_sde_kms(master_priv->kms);
	rc = sde_power_resource_enable(&master_priv->phandle,
		sde_kms->core_client, true);
	if (rc) {
		pr_err("resource enable failed: %d\n", rc);
		goto error;
	}

	/* check bootloader status register */
	lk_status = edrm_splash_get_lk_status(kms);
	if (lk_status == SPLASH_STATUS_RUNNING)
		edrm_kms->lk_running_flag = true;
	else
		edrm_kms->lk_running_flag = false;

	/* if early domain is not start, eDRM cannot initialize
	 * display interface and bridge chip.  Need to return err
	 * ToDo:  implement interface and bridge chip startup functions
	 */
	if (lk_status == SPLASH_STATUS_NOT_START) {
		rc = -EINVAL;
		pr_err("LK does not start, eDRM cannot initialize\n");
		goto power_error;
	}

	/* only unsecure buffer is support for now */
	edrm_kms->aspace = sde_kms->aspace[MSM_SMMU_DOMAIN_UNSECURE];

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	/*
	 * max crtc width is equal to the max mixer width * 2 and max height is
	 * is 4K
	 */
	dev->mode_config.max_width = sde_kms->catalog->max_sspp_linewidth * 2;
	dev->mode_config.max_height = 4096;

	/*
	 * Support format modifiers for compression etc.
	 */
	dev->mode_config.allow_fb_modifiers = true;

	rc = _edrm_kms_drm_obj_init(edrm_kms);
	if (rc) {
		pr_err("drm obj init failed: %d\n", rc);
		goto power_error;
	}

	/* notify main DRM that eDRM is started */
	edrm_display_acquire(kms);

	sde_power_resource_enable(&master_priv->phandle,
				sde_kms->core_client, false);
	return 0;
power_error:
	sde_power_resource_enable(&master_priv->phandle,
			sde_kms->core_client, false);
error:
	return rc;
}

static long edrm_kms_round_pixclk(struct msm_kms *kms, unsigned long rate,
	struct drm_encoder *encoder)
{
	return rate;
}

static const struct msm_kms_funcs edrm_kms_funcs = {
	.hw_init         = edrm_kms_hw_init,
	.postinit        = edrm_kms_postinit,
	.prepare_fence   = edrm_kms_prepare_fence,
	.prepare_commit  = edrm_kms_prepare_commit,
	.commit          = edrm_kms_commit,
	.complete_commit = edrm_kms_complete_commit,
	.wait_for_crtc_commit_done = edrm_kms_wait_for_commit_done,
	.check_modified_format = sde_format_check_modified_format,
	.get_format      = sde_get_msm_format,
	.round_pixclk    = edrm_kms_round_pixclk,
	.destroy         = edrm_kms_destroy,
	.lastclose       = edrm_kms_lastclose,
};

struct msm_kms *msm_edrm_kms_init(struct drm_device *dev)
{
	struct msm_edrm_kms *edrm_kms;
	struct drm_minor *minor;

	if (!dev || !dev->dev_private) {
		pr_err("drm device node invalid\n");
		return ERR_PTR(-EINVAL);
	}

	minor = drm_minor_acquire(0);
	if (IS_ERR_OR_NULL(minor))
		return ERR_PTR(-EINVAL);

	edrm_kms = kzalloc(sizeof(*edrm_kms), GFP_KERNEL);
	if (!edrm_kms) {
		drm_minor_release(minor);
		return ERR_PTR(-ENOMEM);
	}

	msm_kms_init(&edrm_kms->base, &edrm_kms_funcs);
	edrm_kms->dev = dev;
	edrm_kms->master_dev = minor->dev;
	drm_minor_release(minor);

	return &edrm_kms->base;
}
