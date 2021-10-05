// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"[mi_sde_encoder:%s:%d] " fmt, __func__, __LINE__
#include <linux/time.h>

#include "sde_encoder.h"
#include "sde_encoder_phys.h"

#include "mi_sde_encoder.h"
#include "mi_disp_print.h"
#include "mi_disp_feature.h"
#include "mi_dsi_display.h"

#define to_sde_encoder_virt(x) container_of(x, struct sde_encoder_virt, base)

static struct calc_hw_vsync g_calc_hw_vsync[MI_DISP_MAX];

void mi_sde_encoder_save_vsync_info(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_virt *sde_enc;
	struct msm_mode_info *info;
	struct calc_hw_vsync *calc_vsync;
	ktime_t current_ktime;
	u32 diff_us = 0;
	u32 config_vsync_period_us = 0;

	if (!phys_enc || !phys_enc->parent)
		return;

	sde_enc = to_sde_encoder_virt(phys_enc->parent);
	if (sde_enc->disp_info.intf_type != DRM_MODE_CONNECTOR_DSI)
		return;

	if (sde_enc->disp_info.display_type == SDE_CONNECTOR_PRIMARY)
		calc_vsync = &g_calc_hw_vsync[MI_DISP_PRIMARY];
	else if (sde_enc->disp_info.display_type == SDE_CONNECTOR_SECONDARY)
		calc_vsync = &g_calc_hw_vsync[MI_DISP_SECONDARY];
	else
		return;

	info = &sde_enc->mode_info;
	config_vsync_period_us = USEC_PER_SEC / info->frame_rate;

	current_ktime = ktime_get();
	diff_us = (u64)ktime_us_delta(current_ktime, calc_vsync->last_timestamp);
	if ((diff_us * 10) <= (config_vsync_period_us * 9) ||
		(diff_us * 10) >= (config_vsync_period_us * 11)) {
		calc_vsync->last_timestamp = current_ktime;
		return;
	}

	calc_vsync->vsyc_info[calc_vsync->vsync_count].config_fps = info->frame_rate;
	calc_vsync->vsyc_info[calc_vsync->vsync_count].timestamp = current_ktime;
	calc_vsync->vsyc_info[calc_vsync->vsync_count].real_vsync_period_ns =
			ktime_to_ns(ktime_sub(current_ktime, calc_vsync->last_timestamp));

	calc_vsync->last_timestamp = current_ktime;

	calc_vsync->vsync_count++;
	calc_vsync->vsync_count %= MAX_VSYNC_COUNT;
}

ssize_t mi_sde_encoder_calc_hw_vsync_info(struct dsi_display *display,
			char *buf, size_t size)
{
	struct calc_hw_vsync *calc_vsync;
	ktime_t current_time;
	u64 diff_us;
	int i,index;
	u32 fps;
	u64 total_vsync_period_ns = 0;
	u32 count = 0;
	u64 valid_total_vsync_period_ns = 0;
	u32 valid_count = 0;

	if (!display || !display->panel) {
		DISP_ERROR("Invalid display/panel ptr\n");
		return -EINVAL;
	}

	calc_vsync = &g_calc_hw_vsync[mi_get_disp_id(display)];

	index = (calc_vsync->vsync_count == 0) ?
			(MAX_VSYNC_COUNT - 1) : (calc_vsync->vsync_count -1);
	fps = calc_vsync->vsyc_info[index].config_fps;

	current_time = ktime_get();

	for (i = 0; i < MAX_VSYNC_COUNT; i++) {
		if (fps == calc_vsync->vsyc_info[index].config_fps) {
			diff_us = (u64)ktime_us_delta(current_time,
					calc_vsync->vsyc_info[index].timestamp);
			if (diff_us <= USEC_PER_SEC) {
				valid_total_vsync_period_ns +=
					calc_vsync->vsyc_info[index].real_vsync_period_ns;
				valid_count++;
			}
			total_vsync_period_ns +=
					calc_vsync->vsyc_info[index].real_vsync_period_ns;
			count++;
		}
		index = (index == 0) ? (MAX_VSYNC_COUNT - 1) : (index - 1);
	}

	if (valid_count && valid_count > fps / 4) {
		calc_vsync->measured_vsync_period_ns =
				valid_total_vsync_period_ns / valid_count;
	} else {
		calc_vsync->measured_vsync_period_ns =
				total_vsync_period_ns / count;
	}

	/* Multiplying with 1000 to get fps in floating point */
	calc_vsync->measured_fps_x1000 =
			(u32)((NSEC_PER_SEC * 1000) / calc_vsync->measured_vsync_period_ns);
	DISP_INFO("[hw_vsync_info]fps: %d.%d, vsync_period_ns:%lld,"
			" panel_mode:%s, panel_type:%s, average of %d statistics\n",
			calc_vsync->measured_fps_x1000 / 1000,
			calc_vsync->measured_fps_x1000 % 1000,
			calc_vsync->measured_vsync_period_ns,
			(display->panel->panel_mode == DSI_OP_VIDEO_MODE) ? "dsi_video" : "dsi_cmd",
			display->panel->type,
			valid_count ? valid_count : count);

	return scnprintf(buf, size, "fps: %d.%d vsync_period_ns:%lld"
			" panel_mode:%s panel_type:%s\n",
			calc_vsync->measured_fps_x1000 / 1000,
			calc_vsync->measured_fps_x1000 % 1000,
			calc_vsync->measured_vsync_period_ns,
			(display->panel->panel_mode == DSI_OP_VIDEO_MODE) ? "dsi_video" : "dsi_cmd",
			display->panel->type);
}