/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __EXTD_MULTI_CONTROL_H__
#define __EXTD_MULTI_CONTROL_H__

#include "extd_info.h"
#include "disp_session.h"

void external_display_control_init(void);
int external_display_trigger(enum EXTD_TRIGGER_MODE trigger,
			     unsigned int session);
int external_display_suspend(unsigned int session);
int external_display_resume(unsigned int session);
int external_display_wait_for_vsync(void *config, unsigned int session);
int external_display_get_info(void *info, unsigned int session);
int external_display_switch_mode(enum DISP_MODE mode,
				 unsigned int *session_created,
				 unsigned int session);
int external_display_frame_cfg(struct disp_frame_cfg_t *cfg);

#endif
