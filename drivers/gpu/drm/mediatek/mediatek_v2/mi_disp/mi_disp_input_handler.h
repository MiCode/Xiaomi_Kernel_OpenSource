// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 * Copyright (C) 2022 XiaoMi, Inc.
 */

#ifndef _MI_DSI_INPUT_HANDLER_H_
#define _MI_DSI_INPUT_HANDLER_H_
#include <linux/input.h>
#include <linux/types.h>
#include "mi_disp_feature.h"

int mi_disp_input_handler_init(struct disp_display *display, int disp_id);
int mi_disp_input_handler_deinit(struct disp_display *display, int disp_id);
int mi_disp_input_handler_register(void *display, int disp_id, int intf_type);
int mi_disp_input_handler_unregister(void *display, int disp_id, int intf_type);
bool mi_disp_input_is_touch_active(void);

#endif
