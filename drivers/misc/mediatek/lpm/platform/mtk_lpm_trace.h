/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_TRACE_H__
#define __MTK_LPM_TRACE_H__

#include <mtk_lpm_platform.h>

int mtk_lpm_trace_parsing(struct device_node *parent);
int mtk_lpm_trace_instance_get(int type, struct MTK_LPM_PLAT_TRACE *ins);

#endif
