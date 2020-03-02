/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/moduleparam.h>
#include <linux/version.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "drm_pdp_drv.h"

#include "kernel_compatibility.h"

struct pdp_mode_data {
	int hdisplay;
	int vdisplay;
	int vrefresh;
	bool reduced_blanking;
	bool interlaced;
	bool margins;
};

static const struct pdp_mode_data pdp_extra_modes[] = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0))
	{
		.hdisplay = 1280,
		.vdisplay = 720,
		.vrefresh = 60,
		.reduced_blanking = false,
		.interlaced = false,
		.margins = false,
	},
	{
		.hdisplay = 1920,
		.vdisplay = 1080,
		.vrefresh = 60,
		.reduced_blanking = false,
		.interlaced = false,
		.margins = false,
	},
#endif
};

static char preferred_mode_name[DRM_DISPLAY_MODE_LEN] = "\0";

module_param_string(dvi_preferred_mode,
		    preferred_mode_name,
		    DRM_DISPLAY_MODE_LEN,
		    0444);

MODULE_PARM_DESC(dvi_preferred_mode,
		 "Specify the preferred mode (if supported), e.g. 1280x1024.");


static int pdp_dvi_add_extra_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	int num_modes;
	int i;

	for (i = 0, num_modes = 0; i < ARRAY_SIZE(pdp_extra_modes); i++) {
		mode = drm_cvt_mode(connector->dev,
				    pdp_extra_modes[i].hdisplay,
				    pdp_extra_modes[i].vdisplay,
				    pdp_extra_modes[i].vrefresh,
				    pdp_extra_modes[i].reduced_blanking,
				    pdp_extra_modes[i].interlaced,
				    pdp_extra_modes[i].margins);
		if (mode) {
			drm_mode_probed_add(connector, mode);
			num_modes++;
		}
	}

	return num_modes;
}

static int pdp_dvi_connector_helper_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	int num_modes;
	int len = strlen(preferred_mode_name);

	if (len)
		dev_info(dev->dev, "detected dvi_preferred_mode=%s\n",
					preferred_mode_name);
	else
		dev_info(dev->dev, "no dvi_preferred_mode\n");

	num_modes = drm_add_modes_noedid(connector,
					 dev->mode_config.max_width,
					 dev->mode_config.max_height);

	num_modes += pdp_dvi_add_extra_modes(connector);
	if (num_modes) {
		struct drm_display_mode *pref_mode = NULL;

		if (len) {
			struct drm_display_mode *mode;
			struct list_head *entry;

			list_for_each(entry, &connector->probed_modes) {
				mode = list_entry(entry,
						  struct drm_display_mode,
						  head);
				if (!strcmp(mode->name, preferred_mode_name)) {
					pref_mode = mode;
					break;
				}
			}
		}

		if (pref_mode)
			pref_mode->type |= DRM_MODE_TYPE_PREFERRED;
		else
			drm_set_preferred_mode(connector,
					       dev->mode_config.max_width,
					       dev->mode_config.max_height);
	}

	drm_mode_sort(&connector->probed_modes);

	DRM_DEBUG_DRIVER("[CONNECTOR:%d:%s] found %d modes\n",
			 connector->base.id,
			 connector->name,
			 num_modes);

	return num_modes;
}

static int pdp_dvi_connector_helper_mode_valid(struct drm_connector *connector,
					       struct drm_display_mode *mode)
{
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;
	else if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return MODE_NO_DBLESCAN;

	return MODE_OK;
}

static struct drm_encoder *
pdp_dvi_connector_helper_best_encoder(struct drm_connector *connector)
{
	/* Pick the first encoder we find */
	if (connector->encoder_ids[0] != 0) {
		struct drm_encoder *encoder;

		encoder = drm_encoder_find(connector->dev,
					   NULL,
					   connector->encoder_ids[0]);
		if (encoder) {
			DRM_DEBUG_DRIVER("[ENCODER:%d:%s] best for "
					 "[CONNECTOR:%d:%s]\n",
					 encoder->base.id,
					 encoder->name,
					 connector->base.id,
					 connector->name);
			return encoder;
		}
	}

	return NULL;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
static enum drm_connector_status
pdp_dvi_connector_detect(struct drm_connector *connector,
			 bool force)
{
	/*
	 * It appears that there is no way to determine if a monitor
	 * is connected. This needs to be set to connected otherwise
	 * DPMS never gets set to ON.
	 */
	return connector_status_connected;
}
#endif

static void pdp_dvi_connector_destroy(struct drm_connector *connector)
{
	struct pdp_drm_private *dev_priv = connector->dev->dev_private;

	DRM_DEBUG_DRIVER("[CONNECTOR:%d:%s]\n",
			 connector->base.id,
			 connector->name);

	drm_connector_cleanup(connector);

	kfree(connector);
	dev_priv->connector = NULL;
}

static void pdp_dvi_connector_force(struct drm_connector *connector)
{
}

static struct drm_connector_helper_funcs pdp_dvi_connector_helper_funcs = {
	.get_modes = pdp_dvi_connector_helper_get_modes,
	.mode_valid = pdp_dvi_connector_helper_mode_valid,
	.best_encoder = pdp_dvi_connector_helper_best_encoder,
};

static const struct drm_connector_funcs pdp_dvi_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.reset = NULL,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
	.detect = pdp_dvi_connector_detect,
#endif
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = pdp_dvi_connector_destroy,
	.force = pdp_dvi_connector_force,
};


struct drm_connector *
pdp_dvi_connector_create(struct drm_device *dev)
{
	struct drm_connector *connector;

	connector = kzalloc(sizeof(*connector), GFP_KERNEL);
	if (!connector)
		return ERR_PTR(-ENOMEM);

	drm_connector_init(dev,
			   connector,
			   &pdp_dvi_connector_funcs,
			   DRM_MODE_CONNECTOR_DVID);
	drm_connector_helper_add(connector, &pdp_dvi_connector_helper_funcs);

	connector->dpms = DRM_MODE_DPMS_OFF;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	connector->display_info.subpixel_order = SubPixelHorizontalRGB;

	DRM_DEBUG_DRIVER("[CONNECTOR:%d:%s]\n",
			 connector->base.id,
			 connector->name);

	return connector;
}
