// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DISP_ESD_CHECK_H
#define _MI_DISP_ESD_CHECK_H

struct mi_esd_ctx {
	struct task_struct *disp_esd_irq_chk_task;
	wait_queue_head_t err_flag_wq;
	atomic_t err_flag_event;
	int err_flag_irq_gpio;
	int err_flag_irq_flags;
	int err_flag_irq;
	bool err_flag_enabled;
	bool panel_init;
};

void mi_disp_esd_chk_init(struct drm_crtc *crtc);
void mi_disp_esd_chk_deinit(struct drm_crtc *crtc);
void mi_disp_err_flag_esd_check_switch(struct drm_crtc *crtc, bool enable);
#endif
