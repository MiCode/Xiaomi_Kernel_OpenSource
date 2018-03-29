/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __EXTD_MULTI_CONTROL_H__
#define __EXTD_MULTI_CONTROL_H__

#include "extd_info.h"
#include "disp_session.h"

void external_display_control_init(void);
int external_display_config_input(disp_session_input_config *input, int idx, unsigned int session);
int external_display_trigger(EXTD_TRIGGER_MODE trigger, unsigned int session);
int external_display_wait_for_vsync(void *config, unsigned int session);
int external_display_get_info(void *info, unsigned int session);
int external_display_switch_mode(DISP_MODE mode, unsigned int *session_created, unsigned int session);
#endif
