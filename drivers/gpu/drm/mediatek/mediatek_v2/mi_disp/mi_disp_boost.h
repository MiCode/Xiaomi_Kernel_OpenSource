// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 * Copyright (C) 2022 XiaoMi, Inc.
 */

#ifndef _MI_DSI_IDLE_BOOST_H_
#define _MI_DSI_IDLE_BOOST_H_
#include <linux/types.h>
#include "mi_disp_feature.h"

int mi_disp_boost_init(void);
int mi_disp_boost_deinit(void);
int mi_disp_boost_enable(void);
int mi_disp_boost_disable(void);

#endif
