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

#include "msm_drv.h"
#include "msm_mmu.h"

#include "dsi_display.h"
#include "dsi_drm.h"
#include "sde_wb.h"
#include "sde_hdmi.h"
#include "sde_shd.h"

#include "sde_kms.h"
#include "sde_core_irq.h"
#include "sde_formats.h"
#include "sde_hw_vbif.h"
#include "sde_vbif.h"
#include "sde_encoder.h"
#include "sde_plane.h"
#include "sde_crtc.h"
#include "sde_recovery_manager.h"

#define CREATE_TRACE_POINTS
#include "sde_trace.h"

/**
 * Controls size of event log buffer. Specified as a power of 2.
 */
#define SDE_EVTLOG_SIZE	1024

/*
 * To enable overall DRM driver logging
 * # echo 0x2 > /sys/module/drm/parameters/debug
 *
 * To enable DRM driver h/w logging
 * # echo <mask> > /sys/kernel/debug/dri/0/hw_log_mask
 *
 * See sde_hw_mdss.h for h/w logging mask definitions (search for SDE_DBG_MASK_)
 */
#define SDE_DEBUGFS_DIR "msm_sde"
#define SDE_DEBUGFS_HWMASKNAME "hw_log_mask"

static int sde_kms_recovery_callback(int err_code,
	    struct recovery_client_info *client_info);

static struct recovery_client_info info = {
	.name = "sde_kms",
	.recovery_cb = sde_kms_recovery_callback,
	.err_supported[0] = {SDE_UNDERRUN, 0, 0},
	.err_supported[1] = {SDE_VSYNC_MISS, 0, 0},
	.err_supported[2] = {SDE_SMMU_FAULT, 0, 0},
	.no_of_err = 3,
	.handle = NULL,
	.pdata = NULL,
};

/**
 * sdecustom - enable certain driver customizations for sde clients
 *	Enabling this modifies the standard DRM behavior slightly and assumes
 *	that the clients have specific knowledge about the modifications that
 *	are involved, so don't enable this unless you know what you're doing.
 *
 *	Parts of the driver that are affected by this setting may be located by
 *	searching for invocations of the 'sde_is_custom_client()' function.
 *
 *	This is disabled by default.
 */
static bool sdecustom = true;
module_param(sdecustom, bool, 0400);
MODULE_PARM_DESC(sdecustom, "Enable customizations for sde clients");

static int sde_kms_hw_init(struct msm_kms *kms);
static int _sde_kms_mmu_destroy(struct sde_kms *sde_kms);

bool sde_is_custom_client(void)
{
	return sdecustom;
}

#ifdef CONFIG_DEBUG_FS
static int _sde_danger_signal_status(struct seq_file *s,
		bool danger_status)
{
	struct sde_kms *kms = (struct sde_kms *)s->private;
	struct msm_drm_private *priv;
	struct sde_danger_safe_status status;
	int i;

	if (!kms || !kms->dev || !kms->dev->dev_private || !kms->hw_mdp) {
		SDE_ERROR("invalid arg(s)\n");
		return 0;
	}

	priv = kms->dev->dev_private;
	memset(&status, 0, sizeof(struct sde_danger_safe_status));

	sde_power_resource_enable(&priv->phandle, kms->core_client, true);
	if (danger_status) {
		seq_puts(s, "\nDanger signal status:\n");
		if (kms->hw_mdp->ops.get_danger_status)
			kms->hw_mdp->ops.get_danger_status(kms->hw_mdp,
					&status);
	} else {
		seq_puts(s, "\nSafe signal status:\n");
		if (kms->hw_mdp->ops.get_danger_status)
			kms->hw_mdp->ops.get_danger_status(kms->hw_mdp,
					&status);
	}
	sde_power_resource_enable(&priv->phandle, kms->core_client, false);

	seq_printf(s, "MDP     :  0x%x\n", status.mdp);

	for (i = SSPP_VIG0; i < SSPP_MAX; i++)
		seq_printf(s, "SSPP%d   :  0x%x  \t", i - SSPP_VIG0,
				status.sspp[i]);
	seq_puts(s, "\n");

	for (i = WB_0; i < WB_MAX; i++)
		seq_printf(s, "WB%d     :  0x%x  \t", i - WB_0,
				status.wb[i]);
	seq_puts(s, "\n");

	return 0;
}

#define DEFINE_SDE_DEBUGFS_SEQ_FOPS(__prefix)				\
static int __prefix ## _open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, __prefix ## _show, inode->i_private);	\
}									\
static const struct file_operations __prefix ## _fops = {		\
	.owner = THIS_MODULE,						\
	.open = __prefix ## _open,					\
	.release = single_release,					\
	.read = seq_read,						\
	.llseek = seq_lseek,						\
}

static int sde_debugfs_danger_stats_show(struct seq_file *s, void *v)
{
	return _sde_danger_signal_status(s, true);
}
DEFINE_SDE_DEBUGFS_SEQ_FOPS(sde_debugfs_danger_stats);

static int sde_debugfs_safe_stats_show(struct seq_file *s, void *v)
{
	return _sde_danger_signal_status(s, false);
}
DEFINE_SDE_DEBUGFS_SEQ_FOPS(sde_debugfs_safe_stats);

static void sde_debugfs_danger_destroy(struct sde_kms *sde_kms)
{
	debugfs_remove_recursive(sde_kms->debugfs_danger);
	sde_kms->debugfs_danger = NULL;
}

static int sde_debugfs_danger_init(struct sde_kms *sde_kms,
		struct dentry *parent)
{
	sde_kms->debugfs_danger = debugfs_create_dir("danger",
			parent);
	if (!sde_kms->debugfs_danger) {
		SDE_ERROR("failed to create danger debugfs\n");
		return -EINVAL;
	}

	debugfs_create_file("danger_status", 0644, sde_kms->debugfs_danger,
			sde_kms, &sde_debugfs_danger_stats_fops);
	debugfs_create_file("safe_status", 0644, sde_kms->debugfs_danger,
			sde_kms, &sde_debugfs_safe_stats_fops);

	return 0;
}

static int _sde_debugfs_show_regset32(struct seq_file *s, void *data)
{
	struct sde_debugfs_regset32 *regset;
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	struct msm_drm_private *priv;
	void __iomem *base;
	uint32_t i, addr;

	if (!s || !s->private)
		return 0;

	regset = s->private;

	sde_kms = regset->sde_kms;
	if (!sde_kms || !sde_kms->mmio)
		return 0;

	dev = sde_kms->dev;
	if (!dev)
		return 0;

	priv = dev->dev_private;
	if (!priv)
		return 0;

	base = sde_kms->mmio + regset->offset;

	/* insert padding spaces, if needed */
	if (regset->offset & 0xF) {
		seq_printf(s, "[%x]", regset->offset & ~0xF);
		for (i = 0; i < (regset->offset & 0xF); i += 4)
			seq_puts(s, "         ");
	}

	if (sde_power_resource_enable(&priv->phandle,
				sde_kms->core_client, true)) {
		seq_puts(s, "failed to enable sde clocks\n");
		return 0;
	}

	/* main register output */
	for (i = 0; i < regset->blk_len; i += 4) {
		addr = regset->offset + i;
		if ((addr & 0xF) == 0x0)
			seq_printf(s, i ? "\n[%x]" : "[%x]", addr);
		seq_printf(s, " %08x", readl_relaxed(base + i));
	}
	seq_puts(s, "\n");
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

	return 0;
}

static int sde_debugfs_open_regset32(struct inode *inode,
		struct file *file)
{
	return single_open(file, _sde_debugfs_show_regset32, inode->i_private);
}

static const struct file_operations sde_fops_regset32 = {
	.open =		sde_debugfs_open_regset32,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

void sde_debugfs_setup_regset32(struct sde_debugfs_regset32 *regset,
		uint32_t offset, uint32_t length, struct sde_kms *sde_kms)
{
	if (regset) {
		regset->offset = offset;
		regset->blk_len = length;
		regset->sde_kms = sde_kms;
	}
}

void *sde_debugfs_create_regset32(const char *name, umode_t mode,
		void *parent, struct sde_debugfs_regset32 *regset)
{
	if (!name || !regset || !regset->sde_kms || !regset->blk_len)
		return NULL;

	/* make sure offset is a multiple of 4 */
	regset->offset = round_down(regset->offset, 4);

	return debugfs_create_file(name, mode, parent,
			regset, &sde_fops_regset32);
}

void *sde_debugfs_get_root(struct sde_kms *sde_kms)
{
	return sde_kms ? sde_kms->debugfs_root : 0;
}

static int _sde_debugfs_init(struct sde_kms *sde_kms)
{
	void *p;

	p = sde_hw_util_get_log_mask_ptr();

	if (!sde_kms || !p)
		return -EINVAL;

	if (sde_kms->dev && sde_kms->dev->primary)
		sde_kms->debugfs_root = sde_kms->dev->primary->debugfs_root;
	else
		sde_kms->debugfs_root = debugfs_create_dir(SDE_DEBUGFS_DIR, 0);

	/* allow debugfs_root to be NULL */
	debugfs_create_x32(SDE_DEBUGFS_HWMASKNAME,
			0644, sde_kms->debugfs_root, p);

	/* create common folder for debug information */
	sde_kms->debugfs_debug = debugfs_create_dir("debug",
			sde_kms->debugfs_root);
	if (!sde_kms->debugfs_debug)
		SDE_ERROR("failed to create debugfs debug directory\n");

	sde_debugfs_danger_init(sde_kms, sde_kms->debugfs_debug);
	sde_debugfs_vbif_init(sde_kms, sde_kms->debugfs_debug);

	return 0;
}

static void _sde_debugfs_destroy(struct sde_kms *sde_kms)
{
	/* don't need to NULL check debugfs_root */
	if (sde_kms) {
		sde_debugfs_vbif_destroy(sde_kms);
		sde_debugfs_danger_destroy(sde_kms);
		debugfs_remove_recursive(sde_kms->debugfs_debug);
		sde_kms->debugfs_debug = 0;
		debugfs_remove_recursive(sde_kms->debugfs_root);
		sde_kms->debugfs_root = 0;
	}
}
#else
static void sde_debugfs_danger_destroy(struct sde_kms *sde_kms,
		struct dentry *parent)
{
}

static int sde_debugfs_danger_init(struct sde_kms *sde_kms,
		struct dentry *parent)
{
	return 0;
}
#endif

static int sde_kms_enable_vblank(struct msm_kms *kms, struct drm_crtc *crtc)
{
	return sde_crtc_vblank(crtc, true);
}

static void sde_kms_disable_vblank(struct msm_kms *kms, struct drm_crtc *crtc)
{
	sde_crtc_vblank(crtc, false);
}

static void sde_kms_prepare_commit(struct msm_kms *kms,
		struct drm_atomic_state *state)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct drm_device *dev = sde_kms->dev;
	struct msm_drm_private *priv = dev->dev_private;

	sde_power_resource_enable(&priv->phandle,
			sde_kms->core_client, true);

	if (sde_kms->splash_info.handoff &&
		sde_kms->splash_info.display_splash_enabled)
		sde_splash_lk_stop_splash(kms, state);

	shd_display_prepare_commit(sde_kms, state);
}

static void sde_kms_commit(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		if (crtc->state->active) {
			SDE_EVT32(DRMID(crtc));
			sde_crtc_commit_kickoff(crtc);
		}
	}
}

static void sde_kms_complete_commit(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct drm_device *dev = sde_kms->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i)
		sde_crtc_complete_commit(crtc, old_crtc_state);

	shd_display_complete_commit(sde_kms, old_state);

	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);
}

static void sde_kms_wait_for_commit_done(struct msm_kms *kms,
		struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev;
	int ret;

	dev = crtc->dev;
	if (!dev) {
		SDE_ERROR("invalid dev\n");
		return;
	}

	if (!crtc->state->enable) {
		SDE_DEBUG("[crtc:%d] not enable\n", crtc->base.id);
		return;
	}

	if (!crtc->state->active) {
		SDE_DEBUG("[crtc:%d] not active\n", crtc->base.id);
		return;
	}

	ret = drm_crtc_vblank_get(crtc);
	if (ret)
		return;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;
		/*
		 * Wait post-flush if necessary to delay before plane_cleanup
		 * For example, wait for vsync in case of video mode panels
		 * This should be a no-op for command mode panels
		 */
		SDE_EVT32(DRMID(crtc));
		ret = sde_encoder_wait_for_commit_done(encoder);
		if (ret && ret != -EWOULDBLOCK) {
			SDE_ERROR("wait for commit done returned %d\n", ret);
			break;
		}
	}

	drm_crtc_vblank_put(crtc);
}

static void sde_kms_prepare_fence(struct msm_kms *kms,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i, rc;

	if (!kms || !old_state || !old_state->dev || !old_state->acquire_ctx) {
		SDE_ERROR("invalid argument(s)\n");
		return;
	}

retry:
	/* attempt to acquire ww mutex for connection */
	rc = drm_modeset_lock(&old_state->dev->mode_config.connection_mutex,
			       old_state->acquire_ctx);

	if (rc == -EDEADLK) {
		drm_modeset_backoff(old_state->acquire_ctx);
		goto retry;
	}

	/* old_state actually contains updated crtc pointers */
	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i)
		sde_crtc_prepare_commit(crtc, old_crtc_state);
}

/**
 * _sde_kms_get_displays - query for underlying display handles and cache them
 * @sde_kms:    Pointer to sde kms structure
 * Returns:     Zero on success
 */
static int _sde_kms_get_displays(struct sde_kms *sde_kms)
{
	int rc = -ENOMEM;

	if (!sde_kms) {
		SDE_ERROR("invalid sde kms\n");
		return -EINVAL;
	}

	/* dsi */
	sde_kms->dsi_displays = NULL;
	sde_kms->dsi_display_count = dsi_display_get_num_of_displays();
	if (sde_kms->dsi_display_count) {
		sde_kms->dsi_displays = kcalloc(sde_kms->dsi_display_count,
				sizeof(void *),
				GFP_KERNEL);
		if (!sde_kms->dsi_displays) {
			SDE_ERROR("failed to allocate dsi displays\n");
			goto exit_deinit_dsi;
		}
		sde_kms->dsi_display_count =
			dsi_display_get_active_displays(sde_kms->dsi_displays,
					sde_kms->dsi_display_count);
	}

	/* wb */
	sde_kms->wb_displays = NULL;
	sde_kms->wb_display_count = sde_wb_get_num_of_displays();
	if (sde_kms->wb_display_count) {
		sde_kms->wb_displays = kcalloc(sde_kms->wb_display_count,
				sizeof(void *),
				GFP_KERNEL);
		if (!sde_kms->wb_displays) {
			SDE_ERROR("failed to allocate wb displays\n");
			goto exit_deinit_wb;
		}
		sde_kms->wb_display_count =
			wb_display_get_displays(sde_kms->wb_displays,
					sde_kms->wb_display_count);
	}

	/* hdmi */
	sde_kms->hdmi_displays = NULL;
	sde_kms->hdmi_display_count = sde_hdmi_get_num_of_displays();
	SDE_DEBUG("hdmi display count=%d", sde_kms->hdmi_display_count);
	if (sde_kms->hdmi_display_count) {
		sde_kms->hdmi_displays = kcalloc(sde_kms->hdmi_display_count,
				  sizeof(void *),
				  GFP_KERNEL);
		if (!sde_kms->hdmi_displays) {
			SDE_ERROR("failed to allocate hdmi displays\n");
			goto exit_deinit_hdmi;
		}
		sde_kms->hdmi_display_count =
			sde_hdmi_get_displays(sde_kms->hdmi_displays,
				sde_kms->hdmi_display_count);
	}

	/* shd */
	sde_kms->shd_displays = NULL;
	sde_kms->shd_display_count = shd_display_get_num_of_displays();
	if (sde_kms->shd_display_count) {
		sde_kms->shd_displays = kcalloc(sde_kms->shd_display_count,
				sizeof(void *), GFP_KERNEL);
		if (!sde_kms->shd_displays)
			goto exit_deinit_shd;
		sde_kms->shd_display_count =
			shd_display_get_displays(sde_kms->shd_displays,
					sde_kms->shd_display_count);
	}

	return 0;

exit_deinit_shd:
	kfree(sde_kms->shd_displays);
	sde_kms->shd_display_count = 0;
	sde_kms->shd_displays = NULL;
exit_deinit_hdmi:
	sde_kms->hdmi_display_count = 0;
	sde_kms->hdmi_displays = NULL;

exit_deinit_wb:
	kfree(sde_kms->wb_displays);
	sde_kms->wb_display_count = 0;
	sde_kms->wb_displays = NULL;

exit_deinit_dsi:
	kfree(sde_kms->dsi_displays);
	sde_kms->dsi_display_count = 0;
	sde_kms->dsi_displays = NULL;
	return rc;
}

/**
 * _sde_kms_release_displays - release cache of underlying display handles
 * @sde_kms:    Pointer to sde kms structure
 */
static void _sde_kms_release_displays(struct sde_kms *sde_kms)
{
	if (!sde_kms) {
		SDE_ERROR("invalid sde kms\n");
		return;
	}
	kfree(sde_kms->hdmi_displays);
	sde_kms->hdmi_display_count = 0;
	sde_kms->hdmi_displays = NULL;

	kfree(sde_kms->wb_displays);
	sde_kms->wb_displays = NULL;
	sde_kms->wb_display_count = 0;

	kfree(sde_kms->dsi_displays);
	sde_kms->dsi_displays = NULL;
	sde_kms->dsi_display_count = 0;
}

/**
 * _sde_kms_setup_displays - create encoders, bridges and connectors
 *                           for underlying displays
 * @dev:        Pointer to drm device structure
 * @priv:       Pointer to private drm device data
 * @sde_kms:    Pointer to sde kms structure
 * Returns:     Zero on success
 */
static int _sde_kms_setup_displays(struct drm_device *dev,
		struct msm_drm_private *priv,
		struct sde_kms *sde_kms)
{
	static const struct sde_connector_ops dsi_ops = {
		.post_init =  dsi_conn_post_init,
		.detect =     dsi_conn_detect,
		.get_modes =  dsi_connector_get_modes,
		.mode_valid = dsi_conn_mode_valid,
		.get_info =   dsi_display_get_info,
		.set_backlight = dsi_display_set_backlight,
		.set_topology_ctl = dsi_display_set_top_ctl,
	};
	static const struct sde_connector_ops wb_ops = {
		.post_init =    sde_wb_connector_post_init,
		.detect =       sde_wb_connector_detect,
		.get_modes =    sde_wb_connector_get_modes,
		.set_property = sde_wb_connector_set_property,
		.get_info =     sde_wb_get_info,
	};
	static const struct sde_connector_ops hdmi_ops = {
		.pre_deinit = sde_hdmi_connector_pre_deinit,
		.post_init =  sde_hdmi_connector_post_init,
		.detect =     sde_hdmi_connector_detect,
		.get_modes =  sde_hdmi_connector_get_modes,
		.mode_valid = sde_hdmi_mode_valid,
		.get_info =   sde_hdmi_get_info,
		.set_property = sde_hdmi_set_property,
		.get_property = sde_hdmi_get_property,
		.pre_kickoff = sde_hdmi_pre_kickoff,
		.mode_needs_full_range = sde_hdmi_mode_needs_full_range,
		.get_csc_type = sde_hdmi_get_csc_type,
		.set_topology_ctl = sde_hdmi_set_top_ctl,
	};
	static const struct sde_connector_ops shd_ops = {
		.post_init =    shd_connector_post_init,
		.detect =       shd_connector_detect,
		.get_modes =    shd_connector_get_modes,
		.mode_valid =   shd_connector_mode_valid,
		.get_info =     shd_connector_get_info,
	};
	struct msm_display_info info = {0};
	struct drm_encoder *encoder;
	void *display, *connector;
	int i, max_encoders;
	int rc = 0;
	int connector_poll;

	if (!dev || !priv || !sde_kms) {
		SDE_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}

	max_encoders = sde_kms->dsi_display_count +
		sde_kms->wb_display_count +
		sde_kms->hdmi_display_count +
		sde_kms->shd_display_count;

	if (max_encoders > ARRAY_SIZE(priv->encoders)) {
		max_encoders = ARRAY_SIZE(priv->encoders);
		SDE_ERROR("capping number of displays to %d", max_encoders);
	}

	/* dsi */
	for (i = 0; i < sde_kms->dsi_display_count &&
		priv->num_encoders < max_encoders; ++i) {
		display = sde_kms->dsi_displays[i];
		encoder = NULL;

		memset(&info, 0x0, sizeof(info));
		rc = dsi_display_get_info(&info, display);
		if (rc) {
			SDE_ERROR("dsi get_info %d failed\n", i);
			continue;
		}

		encoder = sde_encoder_init(dev, &info);
		if (IS_ERR_OR_NULL(encoder)) {
			SDE_ERROR("encoder init failed for dsi %d\n", i);
			continue;
		}

		rc = sde_splash_setup_display_resource(&sde_kms->splash_info,
					display, DRM_MODE_CONNECTOR_DSI, false);
		if (rc) {
			SDE_ERROR("dsi %d splash resource setup failed %d\n",
									i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		rc = dsi_display_drm_bridge_init(display, encoder);
		if (rc) {
			SDE_ERROR("dsi bridge %d init failed, %d\n", i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		connector = sde_connector_init(dev,
					encoder,
					0,
					display,
					&dsi_ops,
					DRM_CONNECTOR_POLL_HPD,
					DRM_MODE_CONNECTOR_DSI);
		if (connector) {
			priv->encoders[priv->num_encoders++] = encoder;
		} else {
			SDE_ERROR("dsi %d connector init failed\n", i);
			dsi_display_drm_bridge_deinit(display);
			sde_encoder_destroy(encoder);
		}
	}

	/* wb */
	for (i = 0; i < sde_kms->wb_display_count &&
		priv->num_encoders < max_encoders; ++i) {
		display = sde_kms->wb_displays[i];
		encoder = NULL;

		memset(&info, 0x0, sizeof(info));
		rc = sde_wb_get_info(&info, display);
		if (rc) {
			SDE_ERROR("wb get_info %d failed\n", i);
			continue;
		}

		encoder = sde_encoder_init(dev, &info);
		if (IS_ERR_OR_NULL(encoder)) {
			SDE_ERROR("encoder init failed for wb %d\n", i);
			continue;
		}

		rc = sde_wb_drm_init(display, encoder);
		if (rc) {
			SDE_ERROR("wb bridge %d init failed, %d\n", i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		connector = sde_connector_init(dev,
				encoder,
				0,
				display,
				&wb_ops,
				DRM_CONNECTOR_POLL_HPD,
				DRM_MODE_CONNECTOR_VIRTUAL);
		if (connector) {
			priv->encoders[priv->num_encoders++] = encoder;
		} else {
			SDE_ERROR("wb %d connector init failed\n", i);
			sde_wb_drm_deinit(display);
			sde_encoder_destroy(encoder);
		}
	}

	/* hdmi */
	for (i = 0; i < sde_kms->hdmi_display_count &&
		priv->num_encoders < max_encoders; ++i) {
		display = sde_kms->hdmi_displays[i];
		encoder = NULL;

		memset(&info, 0x0, sizeof(info));
		rc = sde_hdmi_dev_init(display);
		if (rc) {
			SDE_ERROR("hdmi dev_init %d failed\n", i);
			continue;
		}
		rc = sde_hdmi_get_info(&info, display);
		if (rc) {
			SDE_ERROR("hdmi get_info %d failed\n", i);
			continue;
		}
		if (info.capabilities & MSM_DISPLAY_CAP_HOT_PLUG)
			connector_poll = DRM_CONNECTOR_POLL_HPD;
		else
			connector_poll = 0;
		encoder = sde_encoder_init(dev, &info);
		if (IS_ERR_OR_NULL(encoder)) {
			SDE_ERROR("encoder init failed for hdmi %d\n", i);
			continue;
		}

		rc = sde_splash_setup_display_resource(&sde_kms->splash_info,
				display, DRM_MODE_CONNECTOR_HDMIA, false);
		if (rc) {
			SDE_ERROR("hdmi %d splash resource setup failed %d\n",
									i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		rc = sde_hdmi_drm_init(display, encoder);
		if (rc) {
			SDE_ERROR("hdmi drm %d init failed, %d\n", i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		connector = sde_connector_init(dev,
					encoder,
					0,
					display,
					&hdmi_ops,
					connector_poll,
					DRM_MODE_CONNECTOR_HDMIA);
		if (connector) {
			priv->encoders[priv->num_encoders++] = encoder;
		} else {
			SDE_ERROR("hdmi %d connector init failed\n", i);
			sde_hdmi_dev_deinit(display);
			sde_hdmi_drm_deinit(display);
			sde_encoder_destroy(encoder);
		}
	}

	/* shd */
	for (i = 0; i < sde_kms->shd_display_count &&
			priv->num_encoders < max_encoders; ++i) {
		display = sde_kms->shd_displays[i];
		encoder = NULL;

		memset(&info, 0x0, sizeof(info));
		rc = shd_connector_get_info(&info, display);
		if (rc) {
			SDE_ERROR("shd get_info %d failed\n", i);
			continue;
		}

		encoder = sde_encoder_init(dev, &info);
		if (IS_ERR_OR_NULL(encoder)) {
			SDE_ERROR("shd encoder init failed %d\n", i);
			continue;
		}

		rc = sde_splash_setup_display_resource(&sde_kms->splash_info,
			display, info.intf_type, true);
		if (rc) {
			SDE_ERROR("shared %d splash res setup failed %d\n",
					i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		rc = shd_drm_bridge_init(display, encoder);
		if (rc) {
			SDE_ERROR("shd bridge %d init failed, %d\n", i, rc);
			sde_encoder_destroy(encoder);
			continue;
		}

		connector = sde_connector_init(dev,
					encoder,
					NULL,
					display,
					&shd_ops,
					DRM_CONNECTOR_POLL_HPD,
					info.intf_type);
		if (connector) {
			priv->encoders[priv->num_encoders++] = encoder;
			priv->connectors[priv->num_connectors++] = connector;
		} else {
			SDE_ERROR("shd %d connector init failed\n", i);
			shd_drm_bridge_deinit(display);
			sde_encoder_destroy(encoder);
		}
	}
	return 0;
}

static void _sde_kms_drm_obj_destroy(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv;
	int i;

	if (!sde_kms) {
		SDE_ERROR("invalid sde_kms\n");
		return;
	} else if (!sde_kms->dev) {
		SDE_ERROR("invalid dev\n");
		return;
	} else if (!sde_kms->dev->dev_private) {
		SDE_ERROR("invalid dev_private\n");
		return;
	}
	priv = sde_kms->dev->dev_private;

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

	_sde_kms_release_displays(sde_kms);
}

static inline int sde_get_crtc_id(const char *display_type)
{
	if (!strcmp(display_type, "primary"))
		return 0;
	else if (!strcmp(display_type, "secondary"))
		return 1;
	else
		return 2;
}

static int _sde_kms_drm_obj_init(struct sde_kms *sde_kms)
{
	struct drm_device *dev;
	struct drm_plane *primary_planes[MAX_PLANES], *plane;
	struct drm_crtc *crtc;

	struct msm_drm_private *priv;
	struct sde_mdss_cfg *catalog;
	struct sde_splash_info *sinfo;

	int primary_planes_idx, i, ret;
	int max_crtc_count, max_plane_count;

	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev) {
		SDE_ERROR("invalid sde_kms\n");
		return -EINVAL;
	}

	dev = sde_kms->dev;
	priv = dev->dev_private;
	catalog = sde_kms->catalog;
	sinfo = &sde_kms->splash_info;

	ret = sde_core_irq_domain_add(sde_kms);
	if (ret)
		goto fail_irq;
	/*
	 * Query for underlying display drivers, and create connectors,
	 * bridges and encoders for them.
	 */
	if (!_sde_kms_get_displays(sde_kms))
		(void)_sde_kms_setup_displays(dev, priv, sde_kms);

	max_crtc_count = min(catalog->mixer_count, priv->num_encoders);

	/* Create the planes */
	primary_planes_idx = 0;
	if (catalog->vp_count) {
		max_plane_count = min_t(u32, catalog->vp_count, MAX_PLANES);

		for (i = 0; i < max_plane_count; i++) {
			bool primary = true;
			int crtc_id =
				sde_get_crtc_id(catalog->vp[i].display_type);

			if (strcmp(catalog->vp[i].plane_type, "primary"))
				primary = false;

			plane = sde_plane_init(dev, catalog->vp[i].id,
					primary, 1UL << crtc_id, true, false);
			if (IS_ERR(plane)) {
				SDE_ERROR("sde_plane_init failed\n");
				ret = PTR_ERR(plane);
				goto fail;
			}
			priv->planes[priv->num_planes++] = plane;

			if (primary) {
				primary_planes[crtc_id] = plane;
				primary_planes_idx++;
			}
		}
	} else {
		max_plane_count = min_t(u32, catalog->sspp_count, MAX_PLANES);

		for (i = 0; i < max_plane_count; i++) {
			bool primary = true;
			bool resv_plane = false;

			if (catalog->sspp[i].features & BIT(SDE_SSPP_CURSOR)
				|| primary_planes_idx >= max_crtc_count)
				primary = false;

			if (sde_splash_query_plane_is_reserved(sinfo,
							catalog->sspp[i].id)) {
				resv_plane = true;
				DRM_INFO("pipe%d is reserved\n",
					catalog->sspp[i].id);
			}

			plane = sde_plane_init(dev, catalog->sspp[i].id,
					primary, (1UL << max_crtc_count) - 1,
					false, resv_plane);
			if (IS_ERR(plane)) {
				SDE_ERROR("sde_plane_init failed\n");
				ret = PTR_ERR(plane);
				goto fail;
			}
			priv->planes[priv->num_planes++] = plane;

			if (primary)
				primary_planes[primary_planes_idx++] = plane;
		}
	}

	max_crtc_count = min(max_crtc_count, primary_planes_idx);

	/* Create one CRTC per encoder */
	for (i = 0; i < max_crtc_count; i++) {
		crtc = sde_crtc_init(dev, primary_planes[i]);
		if (IS_ERR(crtc)) {
			ret = PTR_ERR(crtc);
			goto fail;
		}
		priv->crtcs[priv->num_crtcs++] = crtc;
	}

	if (sde_is_custom_client()) {
		/* All CRTCs are compatible with all planes */
		for (i = 0; i < priv->num_planes; i++)
			priv->planes[i]->possible_crtcs =
				(1 << priv->num_crtcs) - 1;
	}

	/* All CRTCs are compatible with all encoders */
	for (i = 0; i < priv->num_encoders; i++)
		priv->encoders[i]->possible_crtcs = (1 << priv->num_crtcs) - 1;

	return 0;
fail:
	_sde_kms_drm_obj_destroy(sde_kms);
fail_irq:
	sde_core_irq_domain_fini(sde_kms);
	return ret;
}

static int sde_kms_postinit(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct drm_device *dev;

	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev) {
		SDE_ERROR("invalid sde_kms\n");
		return -EINVAL;
	}

	dev = sde_kms->dev;

	/*
	 * Allow vblank interrupt to be disabled by drm vblank timer.
	 */
	dev->vblank_disable_allowed = true;

	shd_display_post_init(sde_kms);

	return 0;
}

static long sde_kms_round_pixclk(struct msm_kms *kms, unsigned long rate,
		struct drm_encoder *encoder)
{
	return rate;
}

static void _sde_kms_hw_destroy(struct sde_kms *sde_kms,
		struct platform_device *pdev)
{
	struct drm_device *dev;
	struct msm_drm_private *priv;
	int i;

	if (!sde_kms || !pdev)
		return;

	dev = sde_kms->dev;
	if (!dev)
		return;

	priv = dev->dev_private;
	if (!priv)
		return;

	if (sde_kms->hw_intr)
		sde_hw_intr_destroy(sde_kms->hw_intr);
	sde_kms->hw_intr = NULL;

	_sde_kms_release_displays(sde_kms);

	/* safe to call these more than once during shutdown */
	_sde_debugfs_destroy(sde_kms);
	_sde_kms_mmu_destroy(sde_kms);
	sde_core_perf_destroy(&sde_kms->perf);

	if (sde_kms->catalog) {
		for (i = 0; i < sde_kms->catalog->vbif_count; i++) {
			u32 vbif_idx = sde_kms->catalog->vbif[i].id;

			if ((vbif_idx < VBIF_MAX) && sde_kms->hw_vbif[vbif_idx])
				sde_hw_vbif_destroy(sde_kms->hw_vbif[vbif_idx]);
		}
	}

	if (sde_kms->rm_init)
		sde_rm_destroy(&sde_kms->rm);
	sde_kms->rm_init = false;

	if (sde_kms->catalog)
		sde_hw_catalog_deinit(sde_kms->catalog);
	sde_kms->catalog = NULL;

	if (sde_kms->splash_info.handoff) {
		if (sde_kms->core_client)
			sde_splash_destroy(&sde_kms->splash_info,
				&priv->phandle, sde_kms->core_client);
	}

	if (sde_kms->core_client)
		sde_power_client_destroy(&priv->phandle,
				sde_kms->core_client);
	sde_kms->core_client = NULL;

	if (sde_kms->vbif[VBIF_NRT])
		msm_iounmap(pdev, sde_kms->vbif[VBIF_NRT]);
	sde_kms->vbif[VBIF_NRT] = NULL;

	if (sde_kms->vbif[VBIF_RT])
		msm_iounmap(pdev, sde_kms->vbif[VBIF_RT]);
	sde_kms->vbif[VBIF_RT] = NULL;

	if (sde_kms->mmio)
		msm_iounmap(pdev, sde_kms->mmio);
	sde_kms->mmio = NULL;
}

static void sde_kms_destroy(struct msm_kms *kms)
{
	struct sde_kms *sde_kms;
	struct drm_device *dev;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		return;
	}

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;
	if (!dev) {
		SDE_ERROR("invalid device\n");
		return;
	}

	sde_recovery_client_unregister(info.handle);
	info.handle = NULL;
	_sde_kms_hw_destroy(sde_kms, dev->platformdev);
	kfree(sde_kms);
}

static void sde_kms_preclose(struct msm_kms *kms, struct drm_file *file)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	struct drm_device *dev = sde_kms->dev;
	struct msm_drm_private *priv = dev->dev_private;
	unsigned i;

	for (i = 0; i < priv->num_crtcs; i++)
		sde_crtc_cancel_pending_flip(priv->crtcs[i], file);
}

static bool sde_kms_early_display_status(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);

	return sde_kms->splash_info.handoff;
}

static const struct msm_kms_funcs kms_funcs = {
	.hw_init         = sde_kms_hw_init,
	.postinit        = sde_kms_postinit,
	.irq_preinstall  = sde_irq_preinstall,
	.irq_postinstall = sde_irq_postinstall,
	.irq_uninstall   = sde_irq_uninstall,
	.irq             = sde_irq,
	.preclose        = sde_kms_preclose,
	.prepare_fence   = sde_kms_prepare_fence,
	.prepare_commit  = sde_kms_prepare_commit,
	.commit          = sde_kms_commit,
	.complete_commit = sde_kms_complete_commit,
	.wait_for_crtc_commit_done = sde_kms_wait_for_commit_done,
	.enable_vblank   = sde_kms_enable_vblank,
	.disable_vblank  = sde_kms_disable_vblank,
	.check_modified_format = sde_format_check_modified_format,
	.get_format      = sde_get_msm_format,
	.round_pixclk    = sde_kms_round_pixclk,
	.destroy         = sde_kms_destroy,
	.early_display_status = sde_kms_early_display_status,
};

/* the caller api needs to turn on clock before calling it */
static inline void _sde_kms_core_hw_rev_init(struct sde_kms *sde_kms)
{
	sde_kms->core_rev = readl_relaxed(sde_kms->mmio + 0x0);
}

static int _sde_kms_mmu_destroy(struct sde_kms *sde_kms)
{
	struct msm_mmu *mmu;
	int i;

	for (i = ARRAY_SIZE(sde_kms->aspace) - 1; i >= 0; i--) {
		if (!sde_kms->aspace[i])
			continue;

		mmu = sde_kms->aspace[i]->mmu;

		mmu->funcs->detach(mmu);
		msm_gem_address_space_put(sde_kms->aspace[i]);

		sde_kms->aspace[i] = NULL;
	}

	return 0;
}

static int sde_smmu_fault_handler(struct iommu_domain *iommu,
	 struct device *dev, unsigned long iova, int flags, void *arg)
{

	dev_info(dev, "%s: iova=0x%08lx, flags=0x%x, iommu=%pK\n", __func__,
			iova, flags, iommu);

	sde_recovery_set_events(SDE_SMMU_FAULT);

	return 0;
}

static int _sde_kms_mmu_init(struct sde_kms *sde_kms)
{
	struct msm_mmu *mmu;
	int i, ret;
	int data = 0;

	for (i = 0; i < MSM_SMMU_DOMAIN_MAX; i++) {
		struct msm_gem_address_space *aspace;

		mmu = msm_smmu_new(sde_kms->dev->dev, i);
		if (IS_ERR(mmu)) {
			/* MMU's can be optional depending on platform */
			ret = PTR_ERR(mmu);
			DRM_INFO("failed to init iommu id %d: rc: %d\n", i,
					ret);
			continue;
		}

		msm_smmu_register_fault_handler(mmu, sde_smmu_fault_handler);

		/* Attaching smmu means IOMMU HW starts to work immediately.
		 * However, display HW in LK is still accessing memory
		 * while the memory map is not done yet.
		 * So first set DOMAIN_ATTR_EARLY_MAP attribute 1 to bypass
		 * stage 1 translation in IOMMU HW.
		 */
		if ((i == MSM_SMMU_DOMAIN_UNSECURE) &&
				sde_kms->splash_info.handoff) {
			ret = mmu->funcs->set_property(mmu,
					DOMAIN_ATTR_EARLY_MAP,
					&sde_kms->splash_info.handoff);
			if (ret) {
				SDE_ERROR("failed to set map att: %d\n", ret);
				mmu->funcs->destroy(mmu);
				goto fail;
			}
		}

		aspace = msm_gem_smmu_address_space_create(sde_kms->dev->dev,
			mmu, "sde");
		if (IS_ERR(aspace)) {
			ret = PTR_ERR(aspace);
			mmu->funcs->destroy(mmu);
			goto fail;
		}

		sde_kms->aspace[i] = aspace;

		ret = mmu->funcs->attach(mmu, NULL, 0);
		if (ret) {
			SDE_ERROR("failed to attach iommu %d: %d\n", i, ret);
			msm_gem_address_space_put(aspace);
			goto fail;
		}

		/*
		 * It's safe now to map the physical memory blcok LK accesses.
		 */
		if ((i == MSM_SMMU_DOMAIN_UNSECURE) &&
				sde_kms->splash_info.handoff) {
			ret = sde_splash_smmu_map(sde_kms->dev, mmu,
					&sde_kms->splash_info);
			if (ret) {
				SDE_ERROR("map rsv mem failed: %d\n", ret);
				msm_gem_address_space_put(aspace);
				goto fail;
			}

			/*
			 * Enable stage 1 smmu after user has finished early
			 * mapping of splash memory.
			 */
			ret = mmu->funcs->set_property(mmu,
					DOMAIN_ATTR_EARLY_MAP,
					&data);
			if (ret) {
				SDE_ERROR("failed to set map att(%d): %d\n",
								data, ret);
				msm_gem_address_space_put(aspace);
				goto fail;
			}
		}
	}

	return 0;
fail:
	_sde_kms_mmu_destroy(sde_kms);

	return ret;
}

static void __iomem *_sde_kms_ioremap(struct platform_device *pdev,
		const char *name, unsigned long *out_size)
{
	struct resource *res;
	unsigned long size;
	void __iomem *ptr;

	if (out_size)
		*out_size = 0;

	if (name)
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	else
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		/* availability depends on platform */
		SDE_DEBUG("failed to get memory resource: %s\n", name);
		return NULL;
	}

	size = resource_size(res);

	ptr = devm_ioremap_nocache(&pdev->dev, res->start, size);
	if (!ptr) {
		SDE_ERROR("failed to ioremap: %s\n", name);
		return NULL;
	}

	SDE_DEBUG("IO:region %s %pK %08lx\n", name, ptr, size);

	if (out_size)
		*out_size = size;

	return ptr;
}


static int sde_kms_hw_init(struct msm_kms *kms)
{
	struct sde_kms *sde_kms;
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct sde_splash_info *sinfo;
	int i, rc = -EINVAL;

	if (!kms) {
		SDE_ERROR("invalid kms\n");
		goto end;
	}

	rc = sde_recovery_client_register(&info);
	if (rc)
		pr_err("%s recovery mgr register failed %d\n",
							__func__, rc);

	sde_kms = to_sde_kms(kms);
	dev = sde_kms->dev;
	if (!dev || !dev->platformdev) {
		SDE_ERROR("invalid device\n");
		goto end;
	}

	priv = dev->dev_private;
	if (!priv) {
		SDE_ERROR("invalid private data\n");
		goto end;
	}

	sde_kms->mmio = _sde_kms_ioremap(dev->platformdev, "mdp_phys",
			&sde_kms->mmio_len);
	if (!sde_kms->mmio) {
		SDE_ERROR("mdp register memory map failed\n");
		goto error;
	}
	DRM_INFO("mapped mdp address space @%pK\n", sde_kms->mmio);

	rc = sde_dbg_reg_register_base(SDE_DBG_NAME, sde_kms->mmio,
			sde_kms->mmio_len);
	if (rc)
		SDE_ERROR("dbg base register kms failed: %d\n", rc);

	sde_kms->vbif[VBIF_RT] = _sde_kms_ioremap(dev->platformdev, "vbif_phys",
			&sde_kms->vbif_len[VBIF_RT]);
	if (!sde_kms->vbif[VBIF_RT]) {
		SDE_ERROR("vbif register memory map failed\n");
		goto error;
	}

	rc = sde_dbg_reg_register_base("vbif_rt", sde_kms->vbif[VBIF_RT],
				sde_kms->vbif_len[VBIF_RT]);
	if (rc)
		SDE_ERROR("dbg base register vbif_rt failed: %d\n", rc);

	sde_kms->vbif[VBIF_NRT] = _sde_kms_ioremap(dev->platformdev,
			"vbif_nrt_phys", &sde_kms->vbif_len[VBIF_NRT]);
	if (!sde_kms->vbif[VBIF_NRT]) {
		SDE_DEBUG("VBIF NRT is not defined");
	} else {
		rc = sde_dbg_reg_register_base("vbif_nrt",
				sde_kms->vbif[VBIF_NRT],
				sde_kms->vbif_len[VBIF_NRT]);
		if (rc)
			SDE_ERROR("dbg base register vbif_nrt failed: %d\n",
					rc);
	}

	sde_kms->core_client = sde_power_client_create(&priv->phandle, "core");
	if (IS_ERR_OR_NULL(sde_kms->core_client)) {
		rc = PTR_ERR(sde_kms->core_client);
		SDE_ERROR("sde power client create failed: %d\n", rc);
		sde_kms->core_client = NULL;
		goto error;
	}

	rc = sde_power_resource_enable(&priv->phandle, sde_kms->core_client,
		true);
	if (rc) {
		SDE_ERROR("resource enable failed: %d\n", rc);
		goto error;
	}

	_sde_kms_core_hw_rev_init(sde_kms);

	pr_info("sde hardware revision:0x%x\n", sde_kms->core_rev);

	sde_kms->catalog = sde_hw_catalog_init(dev, sde_kms->core_rev);
	if (IS_ERR_OR_NULL(sde_kms->catalog)) {
		rc = PTR_ERR(sde_kms->catalog);
		SDE_ERROR("catalog init failed: %d\n", rc);
		sde_kms->catalog = NULL;
		goto power_error;
	}

	sde_dbg_init_dbg_buses(sde_kms->core_rev);

	rc = sde_rm_init(&sde_kms->rm, sde_kms->catalog, sde_kms->mmio,
			sde_kms->dev);
	if (rc) {
		SDE_ERROR("rm init failed: %d\n", rc);
		goto power_error;
	}

	sde_kms->rm_init = true;

	sde_kms->hw_mdp = sde_rm_get_mdp(&sde_kms->rm);
	if (IS_ERR_OR_NULL(sde_kms->hw_mdp)) {
		rc = PTR_ERR(sde_kms->hw_mdp);
		SDE_ERROR("failed to get hw_mdp: %d\n", rc);
		sde_kms->hw_mdp = NULL;
		goto power_error;
	}

	/*
	 * Read the DISP_INTF_SEL register to check
	 * whether early display is enabled in LK.
	 */
	rc = sde_splash_get_handoff_status(kms);
	if (rc) {
		SDE_ERROR("get early splash status failed: %d\n", rc);
		goto power_error;
	}

	/*
	 * when LK has enabled early display, sde_splash_parse_dt and
	 * sde_splash_init must be called. The first function is to parse the
	 * mandatory memory node for splash function, and the second function
	 * will first do bandwidth voting job, because display hardware is now
	 * accessing AHB data bus, otherwise device reboot will happen, and then
	 * to check if the memory is reserved.
	 */
	sinfo = &sde_kms->splash_info;
	if (sinfo->handoff) {
		rc = sde_splash_parse_memory_dt(dev);
		if (rc) {
			SDE_ERROR("parse memory dt failed: %d\n", rc);
			goto power_error;
		}

		rc = sde_splash_parse_reserved_plane_dt(dev, sinfo,
							sde_kms->catalog);
		if (rc)
			SDE_ERROR("parse reserved plane dt failed: %d\n", rc);

		sde_splash_init(&priv->phandle, kms);
	}

	for (i = 0; i < sde_kms->catalog->vbif_count; i++) {
		u32 vbif_idx = sde_kms->catalog->vbif[i].id;

		sde_kms->hw_vbif[i] = sde_hw_vbif_init(vbif_idx,
				sde_kms->vbif[vbif_idx], sde_kms->catalog);
		if (IS_ERR_OR_NULL(sde_kms->hw_vbif[vbif_idx])) {
			rc = PTR_ERR(sde_kms->hw_vbif[vbif_idx]);
			SDE_ERROR("failed to init vbif %d: %d\n", vbif_idx, rc);
			sde_kms->hw_vbif[vbif_idx] = NULL;
			goto power_error;
		}
	}

	/*
	 * Now we need to read the HW catalog and initialize resources such as
	 * clocks, regulators, GDSC/MMAGIC, ioremap the register ranges etc
	 */
	rc = _sde_kms_mmu_init(sde_kms);
	if (rc) {
		SDE_ERROR("sde_kms_mmu_init failed: %d\n", rc);
		goto power_error;
	}

	/*
	 * NOTE: Calling sde_debugfs_init here so that the drm_minor device for
	 *       'primary' is already created.
	 */
	rc = _sde_debugfs_init(sde_kms);
	if (rc) {
		SDE_ERROR("sde_debugfs init failed: %d\n", rc);
		goto power_error;
	}

	rc = sde_core_perf_init(&sde_kms->perf, dev, sde_kms->catalog,
			&priv->phandle, priv->pclient, "core_clk_src",
			sde_kms->debugfs_debug);
	if (rc) {
		SDE_ERROR("failed to init perf %d\n", rc);
		goto perf_err;
	}

	sde_kms->hw_intr = sde_hw_intr_init(sde_kms->mmio, sde_kms->catalog);
	if (IS_ERR_OR_NULL(sde_kms->hw_intr)) {
		rc = PTR_ERR(sde_kms->hw_intr);
		SDE_ERROR("hw_intr init failed: %d\n", rc);
		sde_kms->hw_intr = NULL;
		goto hw_intr_init_err;
	}

	/*
	 * _sde_kms_drm_obj_init should create the DRM related objects
	 * i.e. CRTCs, planes, encoders, connectors and so forth
	 */
	rc = _sde_kms_drm_obj_init(sde_kms);
	if (rc) {
		SDE_ERROR("modeset init failed: %d\n", rc);
		goto drm_obj_init_err;
	}

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	/*
	 * max crtc width is equal to the max mixer width * 2 and max height is
	 * is 4K
	 */
	dev->mode_config.max_width = sde_kms->catalog->max_mixer_width * 2;
	dev->mode_config.max_height = 4096;

	/*
	 * Support format modifiers for compression etc.
	 */
	dev->mode_config.allow_fb_modifiers = true;

	if (!sde_kms->splash_info.handoff)
		sde_power_resource_enable(&priv->phandle,
				sde_kms->core_client, false);

	return 0;

drm_obj_init_err:
	sde_core_perf_destroy(&sde_kms->perf);
hw_intr_init_err:
perf_err:
power_error:
	sde_power_resource_enable(&priv->phandle, sde_kms->core_client, false);
error:
	_sde_kms_hw_destroy(sde_kms, dev->platformdev);
end:
	return rc;
}

static int sde_kms_recovery_callback(int err_code,
	    struct recovery_client_info *client_info)
{
	int rc = 0;

	switch (err_code) {
	case SDE_UNDERRUN:
		pr_debug("%s [SDE_UNDERRUN] error is auto HW receovered\n",
			__func__);
		break;

	case SDE_VSYNC_MISS:
		pr_debug("%s [SDE_VSYNC_MISS] trigger soft reset\n", __func__);
		break;

	case SDE_SMMU_FAULT:
		pr_debug("%s [SDE_SMMU_FAULT] trigger soft reset\n", __func__);
		break;

	default:
		pr_err("%s error %d undefined\n", __func__, err_code);

	}

	return rc;
}

struct msm_kms *sde_kms_init(struct drm_device *dev)
{
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	int rc = 0;

	if (!dev || !dev->dev_private) {
		SDE_ERROR("drm device node invalid\n");
		return ERR_PTR(-EINVAL);
	}

	priv = dev->dev_private;

	sde_kms = kzalloc(sizeof(*sde_kms), GFP_KERNEL);
	if (!sde_kms) {
		SDE_ERROR("failed to allocate sde kms\n");
		return ERR_PTR(-ENOMEM);
	}

	rc = sde_init_recovery_mgr(dev);
	if (rc) {
		SDE_ERROR("Failed SDE recovery mgr Init, err = %d\n", rc);
		kfree(sde_kms);
		return ERR_PTR(-EFAULT);
	}

	msm_kms_init(&sde_kms->base, &kms_funcs);
	sde_kms->dev = dev;

	return &sde_kms->base;
}
