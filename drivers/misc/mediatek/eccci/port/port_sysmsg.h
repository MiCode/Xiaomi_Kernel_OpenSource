/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __PORT_SYSMSG_H__
#define __PORT_SYSMSG_H__
#include "ccci_core.h"
#include "port_t.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
extern void mtk_disp_mipi_clk_change(int msg, unsigned int en);
#endif

#endif	/* __PORT_SYSMSG_H__ */
