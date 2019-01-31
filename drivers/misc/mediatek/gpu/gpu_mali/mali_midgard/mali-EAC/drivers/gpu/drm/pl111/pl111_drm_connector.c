/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * pl111_drm_connector.c
 * Implementation of the connector functions for PL111 DRM
 */
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/version.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/module.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>

#include "pl111_drm.h"


static struct {
	int w, h, type;
} pl111_drm_modes[] = {
	{ 640, 480,  DRM_MODE_TYPE_PREFERRED},
	{ 800, 600,  0},
	{1024, 768,  0},
	{  -1,  -1, -1}
};

void pl111_connector_destroy(struct drm_connector *connector)
{
	struct pl111_drm_connector *pl111_connector =
				PL111_CONNECTOR_FROM_CONNECTOR(connector);

	DRM_DEBUG_KMS("DRM %s on connector=%p\n", __func__, connector);

	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(pl111_connector);
}

enum drm_connector_status pl111_connector_detect(struct drm_connector
							*connector, bool force)
{
	DRM_DEBUG_KMS("DRM %s on connector=%p\n", __func__, connector);
	return connector_status_connected;
}

void pl111_connector_dpms(struct drm_connector *connector, int mode)
{
	DRM_DEBUG_KMS("DRM %s on connector=%p\n", __func__, connector);
}

struct drm_encoder *
pl111_connector_helper_best_encoder(struct drm_connector *connector)
{
	DRM_DEBUG_KMS("DRM %s on connector=%p\n", __func__, connector);

	if (connector->encoder != NULL) {
		return connector->encoder; /* Return attached encoder */
	} else {
		/*
		 * If there is no attached encoder we choose the best candidate
		 * from the list.
		 * For PL111 there is only one encoder so we return the first
		 * one we find.
		 * Other h/w would require a suitable criterion below.
		 */
		struct drm_encoder *encoder = NULL;
		struct drm_device *dev = connector->dev;

		list_for_each_entry(encoder, &dev->mode_config.encoder_list,
					head) {
			if (1) { /* criterion ? */
				break;
			}
		}
		return encoder; /* return best candidate encoder */
	}
}

int pl111_connector_helper_get_modes(struct drm_connector *connector)
{
	int i = 0;
	int count = 0;

	DRM_DEBUG_KMS("DRM %s on connector=%p\n", __func__, connector);

	while (pl111_drm_modes[i].w != -1) {
		struct drm_display_mode *mode =
				drm_mode_find_dmt(connector->dev,
						pl111_drm_modes[i].w,
						pl111_drm_modes[i].h,
						60
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0))
						, false
#endif
						);

		if (mode != NULL) {
			mode->type |= pl111_drm_modes[i].type;
			drm_mode_probed_add(connector, mode);
			count++;
		}

		i++;
	}

	DRM_DEBUG_KMS("found %d modes\n", count);

	return count;
}

int pl111_connector_helper_mode_valid(struct drm_connector *connector,
					struct drm_display_mode *mode)
{
	DRM_DEBUG_KMS("DRM %s on connector=%p\n", __func__, connector);
	return MODE_OK;
}

const struct drm_connector_funcs connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = pl111_connector_destroy,
	.detect = pl111_connector_detect,
	.dpms = pl111_connector_dpms,
};

const struct drm_connector_helper_funcs connector_helper_funcs = {
	.get_modes = pl111_connector_helper_get_modes,
	.mode_valid = pl111_connector_helper_mode_valid,
	.best_encoder = pl111_connector_helper_best_encoder,
};

struct pl111_drm_connector *pl111_connector_create(struct drm_device *dev)
{
	struct pl111_drm_connector *pl111_connector;

	pl111_connector = kzalloc(sizeof(struct pl111_drm_connector),
					GFP_KERNEL);

	if (pl111_connector == NULL) {
		pr_err("Failed to allocated pl111_drm_connector\n");
		return NULL;
	}

	drm_connector_init(dev, &pl111_connector->connector, &connector_funcs,
				DRM_MODE_CONNECTOR_DVII);

	drm_connector_helper_add(&pl111_connector->connector,
					&connector_helper_funcs);

	drm_sysfs_connector_add(&pl111_connector->connector);

	return pl111_connector;
}

