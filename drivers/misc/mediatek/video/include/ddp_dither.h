/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __DDP_DITHER_H__
#define __DDP_DITHER_H__

enum disp_dither_id_t {
	DISP_DITHER0,
	DISP_DITHER1
};

void dither_test(const char *cmd, char *debug_output);

#endif

