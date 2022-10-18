/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_SDE_ENCODER_H_
#define _MI_SDE_ENCODER_H_

#include "sde_encoder_phys.h"
#include "dsi_display.h"

#define MAX_VSYNC_COUNT                   200

struct hw_vsync_info {
	u32 config_fps;
	ktime_t timestamp;
	u64 real_vsync_period_ns;
};

struct calc_hw_vsync {
	struct hw_vsync_info vsyc_info[MAX_VSYNC_COUNT];
	int vsync_count;
	ktime_t last_timestamp;
	u64 measured_vsync_period_ns;
	u64 measured_fps_x1000;
};

void mi_sde_encoder_save_vsync_info(struct sde_encoder_phys *phys_enc);
ssize_t mi_sde_encoder_calc_hw_vsync_info(struct dsi_display *display,
			char *buf, size_t size);

#endif /* _MI_SDE_ENCODER_H_ */
