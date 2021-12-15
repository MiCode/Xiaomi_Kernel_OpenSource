/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_DRM_RECOVERY_H
#define _MTK_DRM_RECOVERY_H

struct mtk_drm_private;

enum mtk_esd_chk_mode {
	READ_EINT,
	READ_LCM,
};

struct mtk_drm_esd_ctx {
	struct task_struct *disp_esd_chk_task;
	wait_queue_head_t check_task_wq;
	wait_queue_head_t ext_te_wq;
	atomic_t ext_te_event;
	atomic_t check_wakeup;
	atomic_t target_time;
	int eint_irq;
	u32 chk_active;
	u32 chk_mode;
	u32 chk_sta;
};

void mtk_disp_esd_check_switch(struct drm_crtc *crtc, bool enable);
void mtk_disp_chk_recover_init(struct drm_crtc *crtc);
long disp_dts_gpio_init(struct device *dev, struct mtk_drm_private *private);

#endif
