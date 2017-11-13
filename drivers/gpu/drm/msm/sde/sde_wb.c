/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <uapi/drm/sde_drm.h>

#include "msm_kms.h"
#include "sde_kms.h"
#include "sde_wb.h"
#include "sde_formats.h"

/* maximum display mode resolution if not available from catalog */
#define SDE_WB_MODE_MAX_WIDTH	4096
#define SDE_WB_MODE_MAX_HEIGHT	4096

/* Serialization lock for sde_wb_list */
static DEFINE_MUTEX(sde_wb_list_lock);

/* List of all writeback devices installed */
static LIST_HEAD(sde_wb_list);

/**
 * sde_wb_is_format_valid - check if given format/modifier is supported
 * @wb_dev:	Pointer to writeback device
 * @pixel_format:	Fourcc pixel format
 * @format_modifier:	Format modifier
 * Returns:		true if valid; false otherwise
 */
static int sde_wb_is_format_valid(struct sde_wb_device *wb_dev,
		u32 pixel_format, u64 format_modifier)
{
	const struct sde_format_extended *fmts = wb_dev->wb_cfg->format_list;
	int i;

	if (!fmts)
		return false;

	for (i = 0; fmts[i].fourcc_format; i++)
		if ((fmts[i].modifier == format_modifier) &&
				(fmts[i].fourcc_format == pixel_format))
			return true;

	return false;
}

enum drm_connector_status
sde_wb_connector_detect(struct drm_connector *connector,
		bool force,
		void *display)
{
	enum drm_connector_status rc = connector_status_unknown;

	SDE_DEBUG("\n");

	if (display)
		rc = ((struct sde_wb_device *)display)->detect_status;

	return rc;
}

int sde_wb_connector_get_modes(struct drm_connector *connector, void *display)
{
	struct sde_wb_device *wb_dev;
	int num_modes = 0;

	if (!connector || !display)
		return 0;

	wb_dev = display;

	SDE_DEBUG("\n");

	mutex_lock(&wb_dev->wb_lock);
	if (wb_dev->count_modes && wb_dev->modes) {
		struct drm_display_mode *mode;
		int i, ret;

		for (i = 0; i < wb_dev->count_modes; i++) {
			mode = drm_mode_create(connector->dev);
			if (!mode) {
				SDE_ERROR("failed to create mode\n");
				break;
			}
			ret = drm_mode_convert_umode(mode,
					&wb_dev->modes[i]);
			if (ret) {
				SDE_ERROR("failed to convert mode %d\n", ret);
				break;
			}

			drm_mode_probed_add(connector, mode);
			num_modes++;
		}
	} else {
		u32 max_width = (wb_dev->wb_cfg && wb_dev->wb_cfg->sblk) ?
				wb_dev->wb_cfg->sblk->maxlinewidth :
				SDE_WB_MODE_MAX_WIDTH;

		num_modes = drm_add_modes_noedid(connector, max_width,
				SDE_WB_MODE_MAX_HEIGHT);
	}
	mutex_unlock(&wb_dev->wb_lock);
	return num_modes;
}

struct drm_framebuffer *
sde_wb_connector_state_get_output_fb(struct drm_connector_state *state)
{
	if (!state || !state->connector ||
		(state->connector->connector_type !=
				DRM_MODE_CONNECTOR_VIRTUAL)) {
		SDE_ERROR("invalid params\n");
		return NULL;
	}

	SDE_DEBUG("\n");

	return sde_connector_get_out_fb(state);
}

int sde_wb_connector_state_get_output_roi(struct drm_connector_state *state,
		struct sde_rect *roi)
{
	if (!state || !roi || !state->connector ||
		(state->connector->connector_type !=
				DRM_MODE_CONNECTOR_VIRTUAL)) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	SDE_DEBUG("\n");

	roi->x = sde_connector_get_property(state, CONNECTOR_PROP_DST_X);
	roi->y = sde_connector_get_property(state, CONNECTOR_PROP_DST_Y);
	roi->w = sde_connector_get_property(state, CONNECTOR_PROP_DST_W);
	roi->h = sde_connector_get_property(state, CONNECTOR_PROP_DST_H);

	return 0;
}

/**
 * sde_wb_connector_set_modes - set writeback modes and connection status
 * @wb_dev:	Pointer to write back device
 * @count_modes:	Count of modes
 * @modes:	Pointer to writeback mode requested
 * @connected:	Connection status requested
 * Returns:	0 if success; error code otherwise
 */
static
int sde_wb_connector_set_modes(struct sde_wb_device *wb_dev,
		u32 count_modes, struct drm_mode_modeinfo __user *modes,
		bool connected)
{
	struct drm_mode_modeinfo *modeinfo = NULL;
	int ret = 0;
	int i;

	if (!wb_dev || !wb_dev->connector ||
			(wb_dev->connector->connector_type !=
			 DRM_MODE_CONNECTOR_VIRTUAL)) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	SDE_DEBUG("\n");

	if (connected) {
		SDE_DEBUG("connect\n");

		if (count_modes && modes) {
			modeinfo = kcalloc(count_modes,
					sizeof(struct drm_mode_modeinfo),
					GFP_KERNEL);
			if (!modeinfo) {
				SDE_ERROR("invalid params\n");
				ret = -ENOMEM;
				goto error;
			}

			if (copy_from_user(modeinfo, modes,
					count_modes *
					sizeof(struct drm_mode_modeinfo))) {
				SDE_ERROR("failed to copy modes\n");
				kfree(modeinfo);
				ret = -EFAULT;
				goto error;
			}

			for (i = 0; i < count_modes; i++) {
				struct drm_display_mode dispmode;

				memset(&dispmode, 0, sizeof(dispmode));
				ret = drm_mode_convert_umode(&dispmode,
						&modeinfo[i]);
				if (ret) {
					SDE_ERROR(
						"failed to convert mode %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x status:%d rc:%d\n",
						i,
						modeinfo[i].name,
						modeinfo[i].vrefresh,
						modeinfo[i].clock,
						modeinfo[i].hdisplay,
						modeinfo[i].hsync_start,
						modeinfo[i].hsync_end,
						modeinfo[i].htotal,
						modeinfo[i].vdisplay,
						modeinfo[i].vsync_start,
						modeinfo[i].vsync_end,
						modeinfo[i].vtotal,
						modeinfo[i].type,
						modeinfo[i].flags,
						dispmode.status,
						ret);
					kfree(modeinfo);
					goto error;
				}
			}
		}

		if (wb_dev->modes) {
			wb_dev->count_modes = 0;

			kfree(wb_dev->modes);
			wb_dev->modes = NULL;
		}

		wb_dev->count_modes = count_modes;
		wb_dev->modes = modeinfo;
		wb_dev->detect_status = connector_status_connected;
	} else {
		SDE_DEBUG("disconnect\n");

		if (wb_dev->modes) {
			wb_dev->count_modes = 0;

			kfree(wb_dev->modes);
			wb_dev->modes = NULL;
		}

		wb_dev->detect_status = connector_status_disconnected;
	}

error:
	return ret;
}

int sde_wb_connector_set_property(struct drm_connector *connector,
		struct drm_connector_state *state,
		int property_index,
		uint64_t value,
		void *display)
{
	struct sde_wb_device *wb_dev = display;
	struct drm_framebuffer *out_fb;
	int rc = 0;

	SDE_DEBUG("\n");

	if (state && (property_index == CONNECTOR_PROP_OUT_FB)) {
		const struct sde_format *sde_format;

		out_fb = sde_connector_get_out_fb(state);
		if (!out_fb)
			goto done;

		sde_format = sde_get_sde_format_ext(out_fb->pixel_format,
				out_fb->modifier,
				drm_format_num_planes(out_fb->pixel_format));
		if (!sde_format) {
			SDE_ERROR("failed to get sde format\n");
			rc = -EINVAL;
			goto done;
		}

		if (!sde_wb_is_format_valid(wb_dev, out_fb->pixel_format,
				out_fb->modifier[0])) {
			SDE_ERROR("unsupported writeback format 0x%x/0x%llx\n",
					out_fb->pixel_format,
					out_fb->modifier[0]);
			rc = -EINVAL;
			goto done;
		}
	}

done:
	return rc;
}

int sde_wb_get_info(struct msm_display_info *info, void *display)
{
	struct sde_wb_device *wb_dev = display;

	if (!info || !wb_dev) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	memset(info, 0, sizeof(struct msm_display_info));
	info->intf_type = DRM_MODE_CONNECTOR_VIRTUAL;
	info->num_of_h_tiles = 1;
	info->h_tile_instance[0] = sde_wb_get_index(display);
	info->is_connected = true;
	info->capabilities = MSM_DISPLAY_CAP_HOT_PLUG | MSM_DISPLAY_CAP_EDID;
	info->max_width = (wb_dev->wb_cfg && wb_dev->wb_cfg->sblk) ?
			wb_dev->wb_cfg->sblk->maxlinewidth :
			SDE_WB_MODE_MAX_WIDTH;
	info->max_height = SDE_WB_MODE_MAX_HEIGHT;
	return 0;
}

int sde_wb_get_mode_info(const struct drm_display_mode *drm_mode,
	struct msm_mode_info *mode_info, u32 max_mixer_width, void *display)
{
	const u32 dual_lm = 2;
	const u32 single_lm = 1;
	const u32 single_intf = 1;
	const u32 no_enc = 0;
	struct msm_display_topology *topology;
	struct sde_wb_device *wb_dev = display;
	u16 hdisplay;
	int i;

	if (!drm_mode || !mode_info || !max_mixer_width || !display) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	hdisplay = drm_mode->hdisplay;

	/* find maximum display width to support */
	for (i = 0; i < wb_dev->count_modes; i++)
		hdisplay = max(hdisplay, wb_dev->modes[i].hdisplay);

	topology = &mode_info->topology;
	topology->num_lm = (max_mixer_width <= hdisplay) ? dual_lm : single_lm;
	topology->num_enc = no_enc;
	topology->num_intf = single_intf;

	mode_info->comp_info.comp_type = MSM_DISPLAY_COMPRESSION_NONE;

	return 0;
}

int sde_wb_connector_set_info_blob(struct drm_connector *connector,
		void *info, void *display, struct msm_mode_info *mode_info)
{
	struct sde_wb_device *wb_dev = display;
	const struct sde_format_extended *format_list;

	if (!connector || !info || !display || !wb_dev->wb_cfg) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	format_list = wb_dev->wb_cfg->format_list;

	/*
	 * Populate info buffer
	 */
	if (format_list) {
		sde_kms_info_start(info, "pixel_formats");
		while (format_list->fourcc_format) {
			sde_kms_info_append_format(info,
					format_list->fourcc_format,
					format_list->modifier);
			++format_list;
		}
		sde_kms_info_stop(info);
	}

	sde_kms_info_add_keyint(info,
			"wb_intf_index",
			wb_dev->wb_idx - WB_0);

	sde_kms_info_add_keyint(info,
			"maxlinewidth",
			wb_dev->wb_cfg->sblk->maxlinewidth);

	sde_kms_info_start(info, "features");
	if (wb_dev->wb_cfg && (wb_dev->wb_cfg->features & SDE_WB_UBWC))
		sde_kms_info_append(info, "wb_ubwc");
	sde_kms_info_stop(info);

	return 0;
}

int sde_wb_connector_post_init(struct drm_connector *connector, void *display)
{
	struct sde_connector *c_conn;
	struct sde_wb_device *wb_dev = display;
	static const struct drm_prop_enum_list e_fb_translation_mode[] = {
		{SDE_DRM_FB_NON_SEC, "non_sec"},
		{SDE_DRM_FB_SEC, "sec"},
	};

	if (!connector || !display || !wb_dev->wb_cfg) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	c_conn = to_sde_connector(connector);
	wb_dev->connector = connector;
	wb_dev->detect_status = connector_status_connected;

	/*
	 * Add extra connector properties
	 */
	msm_property_install_range(&c_conn->property_info, "FB_ID",
			0x0, 0, ~0, 0, CONNECTOR_PROP_OUT_FB);
	msm_property_install_range(&c_conn->property_info, "DST_X",
			0x0, 0, UINT_MAX, 0, CONNECTOR_PROP_DST_X);
	msm_property_install_range(&c_conn->property_info, "DST_Y",
			0x0, 0, UINT_MAX, 0, CONNECTOR_PROP_DST_Y);
	msm_property_install_range(&c_conn->property_info, "DST_W",
			0x0, 0, UINT_MAX, 0, CONNECTOR_PROP_DST_W);
	msm_property_install_range(&c_conn->property_info, "DST_H",
			0x0, 0, UINT_MAX, 0, CONNECTOR_PROP_DST_H);
	msm_property_install_enum(&c_conn->property_info,
			"fb_translation_mode",
			0x0,
			0, e_fb_translation_mode,
			ARRAY_SIZE(e_fb_translation_mode),
			CONNECTOR_PROP_FB_TRANSLATION_MODE);

	return 0;
}

struct drm_framebuffer *sde_wb_get_output_fb(struct sde_wb_device *wb_dev)
{
	struct drm_framebuffer *fb;

	if (!wb_dev || !wb_dev->connector) {
		SDE_ERROR("invalid params\n");
		return NULL;
	}

	SDE_DEBUG("\n");

	mutex_lock(&wb_dev->wb_lock);
	fb = sde_wb_connector_state_get_output_fb(wb_dev->connector->state);
	mutex_unlock(&wb_dev->wb_lock);

	return fb;
}

int sde_wb_get_output_roi(struct sde_wb_device *wb_dev, struct sde_rect *roi)
{
	int rc;

	if (!wb_dev || !wb_dev->connector || !roi) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	SDE_DEBUG("\n");

	mutex_lock(&wb_dev->wb_lock);
	rc = sde_wb_connector_state_get_output_roi(
			wb_dev->connector->state, roi);
	mutex_unlock(&wb_dev->wb_lock);

	return rc;
}

u32 sde_wb_get_num_of_displays(void)
{
	u32 count = 0;
	struct sde_wb_device *wb_dev;

	SDE_DEBUG("\n");

	mutex_lock(&sde_wb_list_lock);
	list_for_each_entry(wb_dev, &sde_wb_list, wb_list) {
		count++;
	}
	mutex_unlock(&sde_wb_list_lock);

	return count;
}

int wb_display_get_displays(void **display_array, u32 max_display_count)
{
	struct sde_wb_device *curr;
	int i = 0;

	SDE_DEBUG("\n");

	if (!display_array || !max_display_count) {
		if (!display_array)
			SDE_ERROR("invalid param\n");
		return 0;
	}

	mutex_lock(&sde_wb_list_lock);
	list_for_each_entry(curr, &sde_wb_list, wb_list) {
		if (i >= max_display_count)
			break;
		display_array[i++] = curr;
	}
	mutex_unlock(&sde_wb_list_lock);

	return i;
}

int sde_wb_config(struct drm_device *drm_dev, void *data,
				struct drm_file *file_priv)
{
	struct sde_drm_wb_cfg *config = data;
	struct msm_drm_private *priv;
	struct sde_wb_device *wb_dev = NULL;
	struct sde_wb_device *curr;
	struct drm_connector *connector;
	uint32_t flags;
	uint32_t connector_id;
	uint32_t count_modes;
	uint64_t modes;
	int rc;

	if (!drm_dev || !data) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	SDE_DEBUG("\n");

	flags = config->flags;
	connector_id = config->connector_id;
	count_modes = config->count_modes;
	modes = config->modes;

	priv = drm_dev->dev_private;

	connector = drm_connector_lookup(drm_dev, connector_id);
	if (!connector) {
		SDE_ERROR("failed to find connector\n");
		rc = -ENOENT;
		goto fail;
	}

	mutex_lock(&sde_wb_list_lock);
	list_for_each_entry(curr, &sde_wb_list, wb_list) {
		if (curr->connector == connector) {
			wb_dev = curr;
			break;
		}
	}
	mutex_unlock(&sde_wb_list_lock);

	if (!wb_dev) {
		SDE_ERROR("failed to find wb device\n");
		rc = -ENOENT;
		goto fail;
	}

	mutex_lock(&wb_dev->wb_lock);

	rc = sde_wb_connector_set_modes(wb_dev, count_modes,
		(struct drm_mode_modeinfo __user *) (uintptr_t) modes,
		(flags & SDE_DRM_WB_CFG_FLAGS_CONNECTED) ? true : false);

	mutex_unlock(&wb_dev->wb_lock);
	drm_helper_hpd_irq_event(drm_dev);
fail:
	return rc;
}

/**
 * _sde_wb_dev_init - perform device initialization
 * @wb_dev:	Pointer to writeback device
 */
static int _sde_wb_dev_init(struct sde_wb_device *wb_dev)
{
	int rc = 0;

	if (!wb_dev) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	SDE_DEBUG("\n");

	return rc;
}

/**
 * _sde_wb_dev_deinit - perform device de-initialization
 * @wb_dev:	Pointer to writeback device
 */
static int _sde_wb_dev_deinit(struct sde_wb_device *wb_dev)
{
	int rc = 0;

	if (!wb_dev) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	SDE_DEBUG("\n");

	return rc;
}

/**
 * sde_wb_bind - bind writeback device with controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 * Returns:     Zero on success
 */
static int sde_wb_bind(struct device *dev, struct device *master, void *data)
{
	struct sde_wb_device *wb_dev;

	if (!dev || !master) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	wb_dev = platform_get_drvdata(to_platform_device(dev));
	if (!wb_dev) {
		SDE_ERROR("invalid wb device\n");
		return -EINVAL;
	}

	SDE_DEBUG("\n");

	mutex_lock(&wb_dev->wb_lock);
	wb_dev->drm_dev = dev_get_drvdata(master);
	mutex_unlock(&wb_dev->wb_lock);

	return 0;
}

/**
 * sde_wb_unbind - unbind writeback from controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 */
static void sde_wb_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct sde_wb_device *wb_dev;

	if (!dev) {
		SDE_ERROR("invalid params\n");
		return;
	}

	wb_dev = platform_get_drvdata(to_platform_device(dev));
	if (!wb_dev) {
		SDE_ERROR("invalid wb device\n");
		return;
	}

	SDE_DEBUG("\n");

	mutex_lock(&wb_dev->wb_lock);
	wb_dev->drm_dev = NULL;
	mutex_unlock(&wb_dev->wb_lock);
}

static const struct component_ops sde_wb_comp_ops = {
	.bind = sde_wb_bind,
	.unbind = sde_wb_unbind,
};

/**
 * sde_wb_drm_init - perform DRM initialization
 * @wb_dev:	Pointer to writeback device
 * @encoder:	Pointer to associated encoder
 */
int sde_wb_drm_init(struct sde_wb_device *wb_dev, struct drm_encoder *encoder)
{
	int rc = 0;

	if (!wb_dev || !wb_dev->drm_dev || !encoder) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	SDE_DEBUG("\n");

	mutex_lock(&wb_dev->wb_lock);

	if (wb_dev->drm_dev->dev_private) {
		struct msm_drm_private *priv = wb_dev->drm_dev->dev_private;
		struct sde_kms *sde_kms = to_sde_kms(priv->kms);

		if (wb_dev->index < sde_kms->catalog->wb_count) {
			wb_dev->wb_idx = sde_kms->catalog->wb[wb_dev->index].id;
			wb_dev->wb_cfg = &sde_kms->catalog->wb[wb_dev->index];
		}
	}

	wb_dev->drm_dev = encoder->dev;
	wb_dev->encoder = encoder;
	mutex_unlock(&wb_dev->wb_lock);
	return rc;
}

int sde_wb_drm_deinit(struct sde_wb_device *wb_dev)
{
	int rc = 0;

	if (!wb_dev) {
		SDE_ERROR("invalid params\n");
		return -EINVAL;
	}

	SDE_DEBUG("\n");

	return rc;
}

/**
 * sde_wb_probe - load writeback module
 * @pdev:	Pointer to platform device
 */
static int sde_wb_probe(struct platform_device *pdev)
{
	struct sde_wb_device *wb_dev;
	int ret;

	wb_dev = devm_kzalloc(&pdev->dev, sizeof(*wb_dev), GFP_KERNEL);
	if (!wb_dev)
		return -ENOMEM;

	SDE_DEBUG("\n");

	ret = of_property_read_u32(pdev->dev.of_node, "cell-index",
			&wb_dev->index);
	if (ret) {
		SDE_DEBUG("cell index not set, default to 0\n");
		wb_dev->index = 0;
	}

	wb_dev->name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!wb_dev->name) {
		SDE_DEBUG("label not set, default to unknown\n");
		wb_dev->name = "unknown";
	}

	wb_dev->wb_idx = SDE_NONE;

	mutex_init(&wb_dev->wb_lock);
	platform_set_drvdata(pdev, wb_dev);

	mutex_lock(&sde_wb_list_lock);
	list_add(&wb_dev->wb_list, &sde_wb_list);
	mutex_unlock(&sde_wb_list_lock);

	if (!_sde_wb_dev_init(wb_dev)) {
		ret = component_add(&pdev->dev, &sde_wb_comp_ops);
		if (ret)
			pr_err("component add failed\n");
	}

	return ret;
}

/**
 * sde_wb_remove - unload writeback module
 * @pdev:	Pointer to platform device
 */
static int sde_wb_remove(struct platform_device *pdev)
{
	struct sde_wb_device *wb_dev;
	struct sde_wb_device *curr, *next;

	wb_dev = platform_get_drvdata(pdev);
	if (!wb_dev)
		return 0;

	SDE_DEBUG("\n");

	(void)_sde_wb_dev_deinit(wb_dev);

	mutex_lock(&sde_wb_list_lock);
	list_for_each_entry_safe(curr, next, &sde_wb_list, wb_list) {
		if (curr == wb_dev) {
			list_del(&wb_dev->wb_list);
			break;
		}
	}
	mutex_unlock(&sde_wb_list_lock);

	kfree(wb_dev->modes);
	mutex_destroy(&wb_dev->wb_lock);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, wb_dev);

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,wb-display"},
	{}
};

static struct platform_driver sde_wb_driver = {
	.probe = sde_wb_probe,
	.remove = sde_wb_remove,
	.driver = {
		.name = "sde_wb",
		.of_match_table = dt_match,
	},
};

static int __init sde_wb_register(void)
{
	return platform_driver_register(&sde_wb_driver);
}

static void __exit sde_wb_unregister(void)
{
	platform_driver_unregister(&sde_wb_driver);
}

module_init(sde_wb_register);
module_exit(sde_wb_unregister);
