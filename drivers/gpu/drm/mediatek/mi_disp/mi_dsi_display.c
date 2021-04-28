/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"mi-dsi-display:[%s] " fmt, __func__

#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_connector.h>

#include "mi_disp_feature_id.h"
#include "mi_dsi_panel.h"
#include "mi_disp_print.h"

//static atomic64_t g_param = ATOMIC64_INIT(0);

int mi_dsi_display_set_disp_param(void *display,
			struct disp_feature_ctl *ctl)
{
	struct mtk_dsi *dsi = (struct mtk_dsi *)display;
	int ret = 0;

	if (!dsi) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	ret = mi_dsi_panel_set_disp_param(dsi, ctl);

	return ret;
}

ssize_t mi_dsi_display_get_disp_param(void *display,
			char *buf, size_t size)
{
	struct mtk_dsi *dsi = (struct mtk_dsi *)display;

	if (!dsi) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}
	return mi_dsi_panel_get_disp_param(dsi, buf, size);
}


ssize_t dsi_display_set_disp_param(struct drm_connector *connector,
			u32 param_type)
{
	int ret = 0;

	if (!connector) {
		pr_err("Invalid display ptr\n");
		return -EINVAL;
	}

	//atomic64_set(&g_param, param_type);
	ret = dsi_panel_set_disp_param(connector, param_type);

	return ret;
}

ssize_t dsi_display_get_disp_param(struct drm_connector *connector,
			char *buf)
{
	//u32 param = (u32)atomic64_read(&g_param);

	//return snprintf(buf, PAGE_SIZE, "0x%08X\n", param);
	if (!connector) {
		pr_err("Invalid display ptr\n");
		return -EINVAL;
	}

	return dsi_panel_get_disp_param(buf);
}


ssize_t dsi_display_write_mipi_reg(struct drm_connector *connector,
			char *buf, size_t count)
{
	if (!connector) {
		pr_err("Invalid connector ptr\n");
		return -EINVAL;
	}

	return dsi_panel_write_mipi_reg(buf, count);
}

ssize_t dsi_display_read_mipi_reg(struct drm_connector *connector,
			char *buf)
{
	if (!connector) {
		pr_err("Invalid connector ptr\n");
		return -EINVAL;
	}

	return dsi_panel_read_mipi_reg(buf);
}

ssize_t mi_dsi_display_write_mipi_reg(void *display, char *buf)
{
	return mi_dsi_panel_write_mipi_reg(buf);
}

ssize_t mi_dsi_display_read_mipi_reg(void *display, char *buf)
{
	return mi_dsi_panel_read_mipi_reg(buf);
}

int mi_dsi_display_write_dsi_cmd(struct dsi_cmd_rw_ctl *ctl)
{
	return mi_dsi_panel_write_dsi_cmd(ctl);
}

int mi_dsi_display_read_dsi_cmd(struct dsi_cmd_rw_ctl *ctl)
{
	return 0;
}

ssize_t mi_dsi_display_write_led_i2c_reg(struct drm_connector *connector,
			char *buf, size_t count)
{
	if (!connector) {
		pr_err("Invalid connector ptr\n");
		return -EINVAL;
	}

	return led_i2c_reg_write(connector, buf, count);
}

ssize_t mi_dsi_display_read_led_i2c_reg(struct drm_connector *connector,
			char *buf)
{
	if (!connector) {
		pr_err("Invalid connector ptr\n");
		return -EINVAL;
	}

	return led_i2c_reg_read(connector, buf);
}

ssize_t mi_dsi_display_read_panel_info(void *display,
			char *buf)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;
	if (!dsi_display) {
		pr_err("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_panel_info(dsi_display, buf);
}

ssize_t mi_dsi_display_read_wp_info(void *display,
			char *buf, size_t size)
{
	return 0;
}

int mi_dsi_display_get_fps(void *display,
			u32 *fps)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display) {
		pr_err("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_fps(display, fps);
}

int mi_dsi_display_set_doze_brightness(void *display, int doze_brightness)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display) {
		pr_err("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_set_doze_brightness(dsi_display, doze_brightness);
}

int mi_dsi_display_get_doze_brightness(void *display,
			u32 *doze_brightness)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_doze_brightness(dsi_display,
				doze_brightness);
}

int mi_dsi_display_set_brightness(void *display, int brightness)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display) {
		pr_err("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_set_brightness(dsi_display, brightness);
}

int mi_dsi_display_get_brightness(void *display,
			u32 *brightness)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	return mi_dsi_panel_get_brightness(dsi_display,
				brightness);
}

int mi_dsi_display_set_brightness_clone(void *display,
			u32 brightness_clone)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;
	int ret = 0;
	struct disp_event event;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	dsi_display->mi_cfg.real_brightness_clone = brightness_clone;

	if (brightness_clone > dsi_display->mi_cfg.thermal_max_brightness_clone)
		brightness_clone = dsi_display->mi_cfg.thermal_max_brightness_clone;

	dsi_display->mi_cfg.brightness_clone = brightness_clone;

	event.disp_id = MI_DISP_PRIMARY;

	event.type = MI_DISP_EVENT_BRIGHTNESS_CLONE;
	event.length = sizeof(brightness_clone);
	mi_disp_feature_event_notify(&event, (u8 *)&brightness_clone);
	return ret;
}

int mi_dsi_display_get_brightness_clone(void *display,
			u32 *brightness_clone)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	*brightness_clone = dsi_display->mi_cfg.brightness_clone;
	return 0;
}

int mi_dsi_display_get_max_brightness_clone(void *display,
			u32 *max_brightness_clone)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	*max_brightness_clone = dsi_display->mi_cfg.max_brightness_clone;
	return 0;
}

int mi_dsi_display_set_thermal_limit_brightness_clone(void *display,
			u32 brightness_clone)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	if (brightness_clone > dsi_display->mi_cfg.max_brightness_clone) {
		DISP_ERROR("thermal max brightness clone out of range!\n");
		return -EINVAL;
	}

	dsi_display->mi_cfg.thermal_max_brightness_clone =
		dsi_display->mi_cfg.max_brightness_clone - brightness_clone;

	mi_dsi_display_set_brightness_clone(dsi_display, dsi_display->mi_cfg.real_brightness_clone);
	return 0;
}

int mi_dsi_display_get_thermal_limit_brightness_clone(void *display,
			u32 *thermal_limit_brightness_clone)
{
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display) {
		DISP_ERROR("Invalid display ptr\n");
		return -EINVAL;
	}

	*thermal_limit_brightness_clone =
		dsi_display->mi_cfg.max_brightness_clone - dsi_display->mi_cfg.thermal_max_brightness_clone;
	return 0;
}

void mi_dsi_display_wakeup_pending_doze_work(void *display)
{
	struct disp_display *dd_ptr;
	struct disp_feature *df = mi_get_disp_feature();
	struct mtk_dsi *dsi_display = (struct mtk_dsi *)display;

	if (!dsi_display || !df) {
		DISP_ERROR("Invalid display or df ptr\n");
		return;
	}

	dd_ptr = &df->d_display[MI_DISP_PRIMARY];
	DISP_INFO("pending_doze_cnt = %d\n", atomic_read(&dd_ptr->pending_doze_cnt));
	if (atomic_read(&dd_ptr->pending_doze_cnt)) {
		DISP_INFO("display wake up pending doze brightness work\n");
		wake_up_all(&dd_ptr->pending_wq);
	}

	return;
}

