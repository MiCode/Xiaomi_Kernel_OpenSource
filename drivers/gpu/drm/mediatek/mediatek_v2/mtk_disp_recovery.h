/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
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
	wait_queue_head_t int_te_wq;
	atomic_t ext_te_event;
	atomic_t int_te_event;
	atomic_t check_wakeup;
	atomic_t target_time;
	int eint_irq;
	u32 chk_active;
	u32 chk_mode;
	u32 chk_sta;
	u32 chk_en;
	int need_release_eint;
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
#ifdef CONFIG_MI_DISP_ESD_CHECK
	struct task_struct *mi_disp_esd_chk_task;
	bool panel_init;
	char esd_read_result [ESD_CHECK_NUM][10];
#endif
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/
};

void mtk_disp_esd_check_switch(struct drm_crtc *crtc, bool enable);
void mtk_disp_chk_recover_init(struct drm_crtc *crtc);
long disp_dts_gpio_init(struct device *dev, struct mtk_drm_private *private);
long _set_state(struct drm_crtc *crtc, const char *name);

#endif
